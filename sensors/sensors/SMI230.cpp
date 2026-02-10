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

#include "SMI230.h"

namespace bosch::sensors {

static constexpr float SMI230_GYRO_VAR = 1.72e-4;  // (rad/s)^2 / Hz

Smi230Acc::Smi230Acc(const std::shared_ptr<ISensorThread> sensorThread) : Accelerometer(sensorThread, mAccelData) {
  mSensorData.driverName = "smi230acc";
  mSensorData.sensorName = "SMI230 BOSCH Accelerometer Sensor";
  mSensorData.propertyIdentifier = "smi230";
  mSensorData.temperatureSysfsRaw = "in_temp_raw";
  mSensorData.minDelayUs = 5000;
  mSensorData.scanType = readScanType(ScanType::DRDY);
  if ((mSensorData.scanType == ScanType::DRDY) || (mSensorData.scanType == ScanType::HW_FIFO)) {
    mSensorData.maxDelayUs = 80000;
  } else {
    mSensorData.maxDelayUs = 2000000;
  }
  mSensorData.power = 0.2f;
  mSensorData.temperatureScale = 0.001f;
  mSensorData.temperatureOffset = 0;
};

Smi230Gyro::Smi230Gyro(const std::shared_ptr<ISensorThread> sensorThread) : Gyroscope(sensorThread, mGyroData) {
  mSensorData.driverName = "smi230gyro";
  mSensorData.sensorName = "SMI230 BOSCH Gyroscope Sensor";
  mSensorData.propertyIdentifier = "smi230";
  mSensorData.minDelayUs = 5000;
  mSensorData.scanType = readScanType(ScanType::DRDY);
  if ((mSensorData.scanType == ScanType::DRDY) || (mSensorData.scanType == ScanType::HW_FIFO)) {
    mSensorData.maxDelayUs = 10000;
  } else {
    mSensorData.maxDelayUs = 2000000;
  }
  mSensorData.power = 5.0f;
}

Smi230LinearAcc::Smi230LinearAcc(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro)
  : LinearAcceleration(accel, gyro) {
  mSensorData.sensorName = "SMI230 BOSCH Linear Accelerometer Sensor";
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.reportMode = CONTINUOUS;

  mGyroVar = SMI230_GYRO_VAR;
}

Smi230Gravity::Smi230Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro)
  : Gravity(accel, gyro) {
  mSensorData.sensorName = "SMI230 BOSCH Gravity Sensor";
  mSensorData.type = BoschSensorType::GRAVITY;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.reportMode = CONTINUOUS;

  mGyroVar = SMI230_GYRO_VAR;
}

Smi230AccUncalibrated::Smi230AccUncalibrated(const std::shared_ptr<SensorCore> base) : Dummy(base) {
  mSensorData.sensorName = "SMI230 BOSCH Accelerometer Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::ACCEL_UNCALIBRATED;
  mSensorData.minDelayUs = 5000;
}

Smi230GyroUncalibrated::Smi230GyroUncalibrated(const std::shared_ptr<SensorCore> base) : Dummy(base) {
  mSensorData.sensorName = "SMI230 BOSCH Gyroscope Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::GYRO_UNCALIBRATED;
  mSensorData.minDelayUs = 5000;
}

}  // namespace bosch::sensors