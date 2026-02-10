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

#include "SMI330.h"

namespace bosch::sensors {

static constexpr float SMI330_GYRO_VAR = 4.9e-5;  // (rad/s)^2 / Hz

Smi330Acc::Smi330Acc(const std::shared_ptr<ISensorThread> sensorThread) : Accelerometer(sensorThread, mAccelData) {
  mSensorData.driverName = "smi330";
  mSensorData.sensorName = "SMI330 BOSCH Accelerometer Sensor";
  mSensorData.propertyIdentifier = "smi330";
  mSensorData.temperatureSysfsRaw = "in_temp_raw";
  mSensorData.minDelayUs = 5000;
  mSensorData.scanType = readScanType(ScanType::DRDY);
  if ((mSensorData.scanType == ScanType::DRDY) || (mSensorData.scanType == ScanType::HW_FIFO)) {
    mSensorData.maxDelayUs = 80000;
  } else {
    mSensorData.maxDelayUs = 2000000;
  }
  mSensorData.power = 0.4f;
  mSensorData.temperatureScale = 1.0f / 512;
  mSensorData.temperatureOffset = 23.0f * 512;
}

Smi330Gyro::Smi330Gyro(const std::shared_ptr<ISensorThread> sensorThread) : Gyroscope(sensorThread, mGyroData) {
  mSensorData.driverName = "smi330";
  mSensorData.sensorName = "SMI330 BOSCH Gyroscope Sensor";
  mSensorData.propertyIdentifier = "smi330";
  mSensorData.temperatureSysfsRaw = "in_temp_raw";
  mSensorData.minDelayUs = 5000;
  mSensorData.scanType = readScanType(ScanType::DRDY);
  if ((mSensorData.scanType == ScanType::DRDY) || (mSensorData.scanType == ScanType::HW_FIFO)) {
    mSensorData.maxDelayUs = 80000;
  } else {
    mSensorData.maxDelayUs = 2000000;
  }
  mSensorData.power = 0.4f;
  mSensorData.temperatureScale = 1.0f / 512;
  mSensorData.temperatureOffset = 23.0f * 512;
}

Smi330LinearAcc::Smi330LinearAcc(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro)
  : LinearAcceleration(accel, gyro) {
  mSensorData.sensorName = "SMI330 BOSCH Linear Accelerometer Sensor";
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.reportMode = CONTINUOUS;

  mGyroVar = SMI330_GYRO_VAR;
}

Smi330Gravity::Smi330Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro)
  : Gravity(accel, gyro) {
  mSensorData.sensorName = "SMI330 BOSCH Gravity Sensor";
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.reportMode = CONTINUOUS;

  mGyroVar = SMI330_GYRO_VAR;
}

Smi330AccUncalibrated::Smi330AccUncalibrated(const std::shared_ptr<SensorCore> base) : Dummy(base) {
  mSensorData.sensorName = "SMI330 BOSCH Accelerometer Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::ACCEL_UNCALIBRATED;
  mSensorData.minDelayUs = 5000;
}

Smi330GyroUncalibrated::Smi330GyroUncalibrated(const std::shared_ptr<SensorCore> base) : Dummy(base) {
  mSensorData.sensorName = "SMI330 BOSCH Gyroscope Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::GYRO_UNCALIBRATED;
  mSensorData.minDelayUs = 5000;
}

}  // namespace bosch::sensors