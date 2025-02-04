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

#include "SMI240.h"

namespace bosch::sensors {

static constexpr float SMI240_GYRO_VAR = 2.25e-4;  // (rad/s)^2 / Hz

Smi240Acc::Smi240Acc() : SensorCore() {
  mSensorData.driverName = "smi240";
  mSensorData.sensorName = "SMI240 BOSCH Accelerometer Sensor";
  mSensorData.sysfsRaw = {"in_accel_x_raw", "in_accel_y_raw", "in_accel_z_raw"};
  mSensorData.temperatureSysfsRaw = "in_temp_object_raw";
  mSensorData.type = BoschSensorType::ACCEL;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 2000000;
  mSensorData.power = 5.0f;
  mSensorData.range = gravityToAcceleration(16);
  mSensorData.resolution = gravityToAcceleration(1.0f / 2000);
  mSensorData.temperatureScale = 1.0f / 256;
  mSensorData.temperatureOffset = 25.0f * 256;
  mSensorData.reportMode = CONTINUOUS;
}

Smi240AccUncalibrated::Smi240AccUncalibrated() {
  mSensorData.sensorName = "SMI240 BOSCH Accelerometer Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::ACCEL_UNCALIBRATED;
}

Smi240Gyro::Smi240Gyro() : SensorCore() {
  mSensorData.driverName = "smi240";
  mSensorData.sensorName = "SMI240 BOSCH Gyroscope Sensor";
  mSensorData.sysfsRaw = {"in_anglvel_x_raw", "in_anglvel_y_raw", "in_anglvel_z_raw"};
  mSensorData.temperatureSysfsRaw = "in_temp_object_raw";
  mSensorData.type = BoschSensorType::GYRO;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 2000000;
  mSensorData.power = 5.0f;
  mSensorData.range = degreeToRad(300);
  mSensorData.resolution = degreeToRad(1.0f / 100);
  mSensorData.temperatureScale = 1.0f / 256;
  mSensorData.temperatureOffset = 25.0f * 256;
  mSensorData.reportMode = CONTINUOUS;
}

Smi240GyroUncalibrated::Smi240GyroUncalibrated() {
  mSensorData.sensorName = "SMI240 BOSCH Gyroscope Uncalibrated Sensor";
  mSensorData.type = BoschSensorType::GYRO_UNCALIBRATED;
}

Smi240LinearAcc::Smi240LinearAcc(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro) {
  mSensorData.sensorName = "SMI240 BOSCH Linear Accelerometer Sensor";
  mSensorData.type = BoschSensorType::LINEAR_ACCEL;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.power = accel->getSensorData().power + gyro->getSensorData().power;
  mSensorData.range = accel->getSensorData().range;
  mSensorData.resolution = accel->getSensorData().resolution;
  mSensorData.reportMode = CONTINUOUS;

  mGyroVar = SMI240_GYRO_VAR;

  mDependencyList.push_back(accel);
  mDependencyList.push_back(gyro);
}

Smi240Gravity::Smi240Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro) {
  mSensorData.sensorName = "SMI240 BOSCH Gravity Sensor";
  mSensorData.type = BoschSensorType::GRAVITY;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.power = accel->getSensorData().power + gyro->getSensorData().power;
  mSensorData.range = accel->getSensorData().range;
  mSensorData.resolution = accel->getSensorData().resolution;
  mSensorData.reportMode = CONTINUOUS;

  mGyroVar = SMI240_GYRO_VAR;

  mDependencyList.push_back(accel);
  mDependencyList.push_back(gyro);
}

}  // namespace bosch::sensors