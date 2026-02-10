/*
 * Copyright (C) 2024-2026 Robert Bosch GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Gyroscope.h"

#include <log/log.h>
#include <unistd.h>

#include "FileHandler.h"

namespace bosch::sensors {

Gyroscope::Gyroscope(const std::shared_ptr<ISensorThread> sensorThread, const GyroData& gyroData)
  : SensorCore(sensorThread), mSensorThread(sensorThread), mGyroData(gyroData) {
  mSensorData.type = BoschSensorType::GYRO;
  mSensorData.reportMode = CONTINUOUS;
}

void Gyroscope::init(int deviceNum) {
  SensorCore::initBase(deviceNum);
  mSensorThread->init(GYRO_IDX, deviceNum, std::bind(&Gyroscope::setOdr, this, std::placeholders::_1),
                      mSensorData.scanType, getRotationMatrix(), mGyroData.sysfsTrigger);
  setScale();
  checkAndUpdateRange();
}

void Gyroscope::enableSensor(bool enable) {
  checkAndUpdateRange();
  mSensorThread->enable(GYRO_IDX, enable, mSysfsPower, enable ? mGyroData.sysfsEnable : mGyroData.sysfsDisable);
}

void Gyroscope::setSamplingRate(int64_t samplingPeriodNs, int64_t reportLatencyNs) {
  mSensorThread->setSamplingRate(GYRO_IDX, samplingPeriodNs, reportLatencyNs);
}

void Gyroscope::setOdr(int64_t samplingPeriodNs) {
  int64_t selectedPeriodNs = us_ns(mGyroData.odrMap.begin()->first);
  std::string sysfsOdr = mGyroData.odrMap.begin()->second;

  for (auto it = mGyroData.odrMap.rbegin(); it != mGyroData.odrMap.rend(); ++it) {
    if (us_ns(it->first) <= samplingPeriodNs) {
      selectedPeriodNs = us_ns(it->first);
      sysfsOdr = it->second;
      break;
    }
  }

  if (mSensorData.scanType == ScanType::SW_TIMER) {
    selectedPeriodNs = samplingPeriodNs;
  }

  SensorCore::updateValidTimestampFactor(selectedPeriodNs);

  ALOGD("Setting gyro ODR to %s", sysfsOdr.c_str());
  writeWithPowerCheck(mSysfsOdr, sysfsOdr);
}

void Gyroscope::setScale() {
  const auto range = readGyroRange(mGyroData.defaultRange);

  if (mRangeMap.find(range) == mRangeMap.end()) {
    ALOGE("Invalid gyro range for scale setting");
    return;
  }
  const std::string& sysfs = mRangeMap.at(range);

  writeWithPowerCheck(mSysfsScale, sysfs);
}

void Gyroscope::checkAndUpdateRange() {
  GyroRange range = mGyroData.defaultRange;
  bosch::hwctl::ReadHandler handler(mDevice, mSysfsScale);
  std::string scale;

  if (handler.read(scale) == 0) {
    for (const auto& [rangeKey, scaleValue] : mRangeMap) {
      if (scale == scaleValue) {
        range = rangeKey;
        break;
      }
    }

    if (range < mGyroData.minRange || range > mGyroData.maxRange) {
      range = mGyroData.defaultRange;
      ALOGE("Invalid gyro scale read from sysfs '%s', use default: %s", scale.c_str(),
            mRangeMap.at(mGyroData.defaultRange).c_str());
    }
  } else {
    ALOGE("Failed to read gyro scale from sysfs");
  }

  mSensorData.range = degreeToRad(static_cast<int>(range));
  mSensorData.resolution = degreeToRad(static_cast<int>(range) / 32768.0f);
  mSensorThread->setResolution(GYRO_IDX, mSensorData.resolution);
}

void Gyroscope::writeWithPowerCheck(const std::string& file, const std::string& content) {
  std::string currentPowerState = mGyroData.sysfsEnable;

  if (mGyroData.powerOnForChange) {
    {
      bosch::hwctl::ReadHandler readHandler(mDevice, mSysfsPower);
      if (readHandler.read(currentPowerState)) {
        ALOGE("Failed to read gyro power state");
      }
    }

    if (currentPowerState != mGyroData.sysfsEnable) {
      bosch::hwctl::WriteHandler(mDevice, mSysfsPower, mGyroData.sysfsEnable);
      usleep(100000);
    }
  }

  bosch::hwctl::WriteReadbackHandler(mDevice, file, content);

  if (mGyroData.powerOnForChange && (currentPowerState == mGyroData.sysfsDisable)) {
    bosch::hwctl::WriteHandler(mDevice, mSysfsPower, mGyroData.sysfsDisable);
  }
}

}  // namespace bosch::sensors