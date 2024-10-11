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

#include "SMI330.h"

#include <unistd.h>

#include "hwctl.h"

namespace bosch::sensors {

static constexpr float SMI330_GYRO_VAR = 4.9e-5;  // (rad/s)^2 / Hz

void Smi330Imu::setPowerMode(Index idx, bool enable, const std::string& device) {
  mIsEnabled[idx] = enable;
  updateSamplingRate(device);
  hwctl::writeToFile(device + mSysfsPowerMode[idx] + std::to_string(enable ? 3 : 0));
  usleep(200000);
}

void Smi330Imu::setSamplingRate(Index idx, int64_t samplingPeriodNs, const std::string& device) {
  mSamplingPeriodNs[idx] = samplingPeriodNs;
  updateSamplingRate(device);
}

void Smi330Imu::updateSamplingRate(const std::string& device) {
  int64_t samplingPeriodNs = 0;
  if (mIsEnabled[Index::ACCEL] && mIsEnabled[Index::GYRO]) {
    samplingPeriodNs = std::min(mSamplingPeriodNs[Index::ACCEL], mSamplingPeriodNs[Index::GYRO]);
  } else if (mIsEnabled[Index::ACCEL]) {
    samplingPeriodNs = mSamplingPeriodNs[Index::ACCEL];
  } else if (mIsEnabled[Index::GYRO]) {
    samplingPeriodNs = mSamplingPeriodNs[Index::GYRO];
  } else {
    return;
  }

  int64_t odr = 1e9 / mMaxSamplingRateNs;
  for (int64_t samplingRate = mMaxSamplingRateNs; samplingRate >= mMinSamplingRateNs; samplingRate /= 2) {
    if (samplingPeriodNs >= samplingRate) {
      odr = 1e9 / samplingRate;
      break;
    }
  }
  hwctl::writeToFile(device + mSysfsOdr + std::to_string(odr));
}

Smi330Acc::Smi330Acc() {
  mSensorData.driverName = "smi330";
  mSensorData.sensorName = "SMI330 BOSCH Accelerometer Sensor";
  mSensorData.sysfsRaw = {"in_accel_x_raw", "in_accel_y_raw", "in_accel_z_raw"};
  mSensorData.type = BoschSensorType::ACCEL;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 1280000;
  mSensorData.power = 0.4f;
  mSensorData.range = gravityToAcceleration(8);
  mSensorData.resolution = gravityToAcceleration(1.0f / 4096);
}

Smi330Gyro::Smi330Gyro() {
  mSensorData.driverName = "smi330";
  mSensorData.sensorName = "SMI330 BOSCH Gyroscope Sensor";
  mSensorData.sysfsRaw = {"in_anglvel_x_raw", "in_anglvel_y_raw", "in_anglvel_z_raw"};
  mSensorData.type = BoschSensorType::GYRO;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 1280000;
  mSensorData.power = 0.4f;
  mSensorData.range = degreeToRad(125);
  mSensorData.resolution = degreeToRad(1.0f / 262.144f);
}

Smi330LinearAcc::Smi330LinearAcc(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro) {
  mSensorData.sensorName = "SMI330 BOSCH Linear Accelerometer Sensor";
  mSensorData.type = BoschSensorType::LINEAR_ACCEL;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.power = accel->getSensorData().power + gyro->getSensorData().power;
  mSensorData.range = accel->getSensorData().range;
  mSensorData.resolution = accel->getSensorData().resolution;
  mGyroVar = SMI330_GYRO_VAR;

  mDependencyList.push_back(accel);
  mDependencyList.push_back(gyro);
}

Smi330Gravity::Smi330Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro) {
  mSensorData.sensorName = "SMI330 BOSCH Gravity Sensor";
  mSensorData.type = BoschSensorType::GRAVITY;
  mSensorData.minDelayUs = 5000;
  mSensorData.maxDelayUs = 20000;
  mSensorData.power = accel->getSensorData().power + gyro->getSensorData().power;
  mSensorData.range = accel->getSensorData().range;
  mSensorData.resolution = accel->getSensorData().resolution;
  mGyroVar = SMI330_GYRO_VAR;

  mDependencyList.push_back(accel);
  mDependencyList.push_back(gyro);
}

}  // namespace bosch::sensors