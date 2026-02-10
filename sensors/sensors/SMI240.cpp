/*
 * Copyright (C) 2024,2026 Robert Bosch GmbH
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

#include "SMI240.h"

#include <log/log.h>

namespace bosch::sensors {

static constexpr float SMI240_GYRO_VAR = 2.25e-4;  // (rad/s)^2 / Hz

Smi240Acc::Smi240Acc(const std::shared_ptr<ISensorThread> sensorThread)
  : SensorCore(sensorThread), mSensorThread(sensorThread) {
  mSensorData.driverName = "smi240";
  mSensorData.sensorName = "SMI240 BOSCH Accelerometer Sensor";
  mSensorData.propertyIdentifier = "smi240";
  mSensorData.temperatureSysfsRaw = "in_temp_raw";
  mSensorData.type = BoschSensorType::ACCEL;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 2000000;
  mSensorData.power = 5.0f;
  mSensorData.range = gravityToAcceleration(16);
  mSensorData.resolution = gravityToAcceleration(1.0f / 2000);
  mSensorData.temperatureScale = 1.0f / 256;
  mSensorData.temperatureOffset = 25.0f * 256;
  mSensorData.reportMode = CONTINUOUS;
  mSensorData.scanType = readScanType(ScanType::SW_TIMER);

  if ((mSensorData.scanType == ScanType::DRDY) || (mSensorData.scanType == ScanType::HW_FIFO)) {
    ALOGW("%s does not support hardware interrupts, falling back to SW_TIMER", mSensorData.sensorName.c_str());
    mSensorData.scanType = ScanType::SW_TIMER;
  }
}

void Smi240Acc::init(int deviceNum) {
  SensorCore::initBase(deviceNum);
  mSensorThread->init(ACCEL_IDX, deviceNum,
                      std::bind(&SensorCore::updateValidTimestampFactor, this, std::placeholders::_1),
                      mSensorData.scanType, getRotationMatrix());
  mSensorThread->setResolution(ACCEL_IDX, mSensorData.resolution);
}

void Smi240Acc::enableSensor(bool enable) { mSensorThread->enable(ACCEL_IDX, enable, "", ""); }

void Smi240Acc::setSamplingRate(int64_t samplingPeriodNs, int64_t reportLatencyNs) {
  mSensorThread->setSamplingRate(ACCEL_IDX, samplingPeriodNs, reportLatencyNs);
}

Smi240Gyro::Smi240Gyro(const std::shared_ptr<ISensorThread> sensorThread)
  : SensorCore(sensorThread), mSensorThread(sensorThread) {
  mSensorData.driverName = "smi240";
  mSensorData.sensorName = "SMI240 BOSCH Gyroscope Sensor";
  mSensorData.propertyIdentifier = "smi240";
  mSensorData.temperatureSysfsRaw = "in_temp_raw";
  mSensorData.type = BoschSensorType::GYRO;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 2000000;
  mSensorData.power = 5.0f;
  mSensorData.range = degreeToRad(300);
  mSensorData.resolution = degreeToRad(1.0f / 100);
  mSensorData.temperatureScale = 1.0f / 256;
  mSensorData.temperatureOffset = 25.0f * 256;
  mSensorData.reportMode = CONTINUOUS;
  mSensorData.scanType = readScanType(ScanType::SW_TIMER);

  if ((mSensorData.scanType == ScanType::DRDY) || (mSensorData.scanType == ScanType::HW_FIFO)) {
    ALOGW("%s does not support hardware interrupts, falling back to SW_TIMER", mSensorData.sensorName.c_str());
    mSensorData.scanType = ScanType::SW_TIMER;
  }
}

void Smi240Gyro::init(int deviceNum) {
  SensorCore::initBase(deviceNum);
  mSensorThread->init(GYRO_IDX, deviceNum,
                      std::bind(&SensorCore::updateValidTimestampFactor, this, std::placeholders::_1),
                      mSensorData.scanType, getRotationMatrix());
  mSensorThread->setResolution(GYRO_IDX, mSensorData.resolution);
}

void Smi240Gyro::enableSensor(bool enable) { mSensorThread->enable(GYRO_IDX, enable, "", ""); }

void Smi240Gyro::setSamplingRate(int64_t samplingPeriodNs, int64_t reportLatencyNs) {
  mSensorThread->setSamplingRate(GYRO_IDX, samplingPeriodNs, reportLatencyNs);
}

Smi240LinearAcc::Smi240LinearAcc(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro)
  : LinearAcceleration(accel, gyro) {
  mSensorData.sensorName = "SMI240 BOSCH Linear Accelerometer Sensor";
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.reportMode = CONTINUOUS;

  mGyroVar = SMI240_GYRO_VAR;
}

Smi240Gravity::Smi240Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro)
  : Gravity(accel, gyro) {
  mSensorData.sensorName = "SMI240 BOSCH Gravity Sensor";
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.reportMode = CONTINUOUS;

  mGyroVar = SMI240_GYRO_VAR;
}

Smi240AccUncalibrated::Smi240AccUncalibrated(const std::shared_ptr<SensorCore> base) : Dummy(base) {
  mSensorData.sensorName = "SMI240 BOSCH Accelerometer Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::ACCEL_UNCALIBRATED;
  mSensorData.minDelayUs = 5000;
}

Smi240GyroUncalibrated::Smi240GyroUncalibrated(const std::shared_ptr<SensorCore> base) : Dummy(base) {
  mSensorData.sensorName = "SMI240 BOSCH Gyroscope Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::GYRO_UNCALIBRATED;
  mSensorData.minDelayUs = 5000;
}

}  // namespace bosch::sensors