/*
 * Copyright (C) 2023 Robert Bosch GmbH
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

#include <unistd.h>

#include "FileHandler.h"

namespace bosch::sensors {

static constexpr float SMI230_GYRO_VAR = 1.72e-4;  // (rad/s)^2 / Hz

Smi230Acc::Smi230Acc() {
  mSensorData.driverName = "smi230acc";
  mSensorData.sensorName = "SMI230 BOSCH Accelerometer Sensor";
  mSensorData.sysfsRaw = {"in_accel_x_raw", "in_accel_y_raw", "in_accel_z_raw"};
  mSensorData.temperatureSysfsRaw = "in_temp_object_raw";
  mSensorData.type = BoschSensorType::ACCEL;
  mSensorData.minDelayUs = 10000;
  mSensorData.maxDelayUs = 2000000;
  mSensorData.power = 0.2f;
  mSensorData.range = gravityToAcceleration(4);
  mSensorData.resolution = gravityToAcceleration(1.0f / 8192);
  mSensorData.temperatureScale = 0.001f;
  mSensorData.temperatureOffset = 0;
  mSensorData.reportMode = CONTINUOUS;
};

void Smi230Acc::setPowerMode(bool enable) {
  { bosch::hwctl::WriteHandler handler(mDevice, mSysfsOdr, "200Hz"); }
  { bosch::hwctl::WriteHandler handler(mDevice, mSysfsPowerMode, enable ? "normal" : "suspend"); }
  usleep(200000);
}

Smi230AccUncalibrated::Smi230AccUncalibrated() {
  mSensorData.sensorName = "SMI230 BOSCH Accelerometer Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::ACCEL_UNCALIBRATED;
}

Smi230Gyro::Smi230Gyro() {
  mSensorData.driverName = "smi230gyro";
  mSensorData.sensorName = "SMI230 BOSCH Gyroscope Sensor";
  mSensorData.sysfsRaw = {"in_anglvel_x_raw", "in_anglvel_y_raw", "in_anglvel_z_raw"};
  mSensorData.type = BoschSensorType::GYRO;
  mSensorData.minDelayUs = 10000;
  mSensorData.maxDelayUs = 2000000;
  mSensorData.power = 5.0f;
  mSensorData.range = degreeToRad(2000);
  mSensorData.resolution = degreeToRad(1.0f / 16.38f);
  mSensorData.reportMode = CONTINUOUS;
}

void Smi230Gyro::setPowerMode(bool enable) {
  { bosch::hwctl::WriteHandler handler(mDevice, mSysfsOdr, "bw64_odr200"); }
  { bosch::hwctl::WriteHandler handler(mDevice, mSysfsPowerMode, enable ? "normal" : "suspend"); }
  usleep(200000);
}

Smi230GyroUncalibrated::Smi230GyroUncalibrated() {
  mSensorData.sensorName = "SMI230 BOSCH Gyroscope Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::GYRO_UNCALIBRATED;
}

Smi230LinearAcc::Smi230LinearAcc(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro) {
  mSensorData.sensorName = "SMI230 BOSCH Linear Accelerometer Sensor";
  mSensorData.type = BoschSensorType::LINEAR_ACCEL;
  mSensorData.minDelayUs = 10000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.power = accel->getSensorData().power + gyro->getSensorData().power;
  mSensorData.range = accel->getSensorData().range;
  mSensorData.resolution = accel->getSensorData().resolution;
  mSensorData.reportMode = CONTINUOUS;

  mGyroVar = SMI230_GYRO_VAR;

  mDependencyList.push_back(accel);
  mDependencyList.push_back(gyro);
}

Smi230Gravity::Smi230Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro) {
  mSensorData.sensorName = "SMI230 BOSCH Gravity Sensor";
  mSensorData.type = BoschSensorType::GRAVITY;
  mSensorData.minDelayUs = 10000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.power = accel->getSensorData().power + gyro->getSensorData().power;
  mSensorData.range = accel->getSensorData().range;
  mSensorData.resolution = accel->getSensorData().resolution;
  mSensorData.reportMode = CONTINUOUS;
  mGyroVar = SMI230_GYRO_VAR;

  mDependencyList.push_back(accel);
  mDependencyList.push_back(gyro);
}

}  // namespace bosch::sensors