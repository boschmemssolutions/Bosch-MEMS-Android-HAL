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

#include "Accelerometer.h"

#include <log/log.h>
#include <unistd.h>

#include "FileHandler.h"

namespace bosch::sensors {

Accelerometer::Accelerometer(const std::shared_ptr<ISensorThread> sensorThread, const AccelData& accelData)
  : SensorCore(sensorThread), mSensorThread(sensorThread), mAccelData(accelData) {
  mSensorData.type = BoschSensorType::ACCEL;
  mSensorData.reportMode = CONTINUOUS;
}

void Accelerometer::init(int deviceNum) {
  SensorCore::initBase(deviceNum);
  mSensorThread->init(ACCEL_IDX, deviceNum, std::bind(&Accelerometer::setOdr, this, std::placeholders::_1),
                      mSensorData.scanType, getRotationMatrix(), mAccelData.sysfsTrigger);
  setScale();
  checkAndUpdateRange();
}

void Accelerometer::enableSensor(bool enable) {
  checkAndUpdateRange();
  mSensorThread->enable(ACCEL_IDX, enable, mSysfsPower, enable ? mAccelData.sysfsEnable : mAccelData.sysfsDisable);
}

void Accelerometer::setSamplingRate(int64_t samplingPeriodNs, int64_t reportLatencyNs) {
  mSensorThread->setSamplingRate(ACCEL_IDX, samplingPeriodNs, reportLatencyNs);
}

void Accelerometer::setOdr(int64_t samplingPeriodNs) {
  int64_t selectedPeriodNs = us_ns(mAccelData.odrMap.begin()->first);
  std::string sysfsOdr = mAccelData.odrMap.begin()->second;

  for (auto it = mAccelData.odrMap.rbegin(); it != mAccelData.odrMap.rend(); ++it) {
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

  ALOGD("Setting accel ODR to %s", sysfsOdr.c_str());
  bosch::hwctl::WriteReadbackHandler(mDevice, mSysfsOdr, sysfsOdr);
}

void Accelerometer::setScale() {
  const auto range = readAccelRange(mAccelData.defaultRange);

  if (mRangeMap.find(range) == mRangeMap.end()) {
    ALOGE("Invalid accel range for scale setting");
    return;
  }
  const std::string& sysfs = mRangeMap.at(range);

  bosch::hwctl::WriteReadbackHandler(mDevice, mSysfsScale, sysfs);
  usleep(10);
}

void Accelerometer::checkAndUpdateRange() {
  AccelRange range = mAccelData.defaultRange;
  bosch::hwctl::ReadHandler handler(mDevice, mSysfsScale);
  std::string scale;

  if (handler.read(scale) == 0) {
    for (const auto& [rangeKey, scaleValue] : mRangeMap) {
      if (scale == scaleValue) {
        range = rangeKey;
        break;
      }
    }

    if (range < mAccelData.minRange || range > mAccelData.maxRange) {
      range = mAccelData.defaultRange;
      ALOGE("Invalid accel scale read from sysfs '%s', use default: %s", scale.c_str(),
            mRangeMap.at(mAccelData.defaultRange).c_str());
    }
  } else {
    ALOGE("Failed to read accel scale from sysfs");
  }

  mSensorData.range = gravityToAcceleration(static_cast<int>(range));
  mSensorData.resolution = gravityToAcceleration(static_cast<int>(range) / 32768.0f);
  mSensorThread->setResolution(ACCEL_IDX, mSensorData.resolution);
}

}  // namespace bosch::sensors