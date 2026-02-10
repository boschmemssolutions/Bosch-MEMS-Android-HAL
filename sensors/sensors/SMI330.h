/*
 * Copyright (C) 2024 Robert Bosch GmbH
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

#ifndef ANDROID_HARDWARE_BOSCH_SENSORS_SMI330_H
#define ANDROID_HARDWARE_BOSCH_SENSORS_SMI330_H

#include "Accelerometer.h"
#include "CompositeSensors.h"
#include "Gyroscope.h"

namespace bosch {
namespace sensors {

class Smi330Acc : public Accelerometer {
public:
  Smi330Acc(const std::shared_ptr<ISensorThread> sensorThread);
  ~Smi330Acc() = default;

private:
  const AccelData mAccelData = {
    .sysfsEnable = "7",
    .sysfsDisable = "0",
    .sysfsTrigger = "smi330-trigger",
    .defaultRange = AccelRange::RANGE_8G,
    .minRange = AccelRange::RANGE_2G,
    .maxRange = AccelRange::RANGE_16G,
    .odrMap =
      {
        {80000, "12"},
        {40000, "25"},
        {20000, "50"},
        {10000, "100"},
        {5000, "200"},
        {2500, "400"},
        {1250, "800"},
        {625, "1600"},
      },
  };
};

class Smi330Gyro : public Gyroscope {
public:
  Smi330Gyro(const std::shared_ptr<ISensorThread> sensorThread);
  ~Smi330Gyro() = default;

private:
  const GyroData mGyroData = {
    .sysfsEnable = "7",
    .sysfsDisable = "0",
    .sysfsTrigger = "smi330-trigger",
    .defaultRange = GyroRange::RANGE_250_DEG_S,
    .minRange = GyroRange::RANGE_125_DEG_S,
    .maxRange = GyroRange::RANGE_500_DEG_S,
    .odrMap =
      {
        {80000, "12"},
        {40000, "25"},
        {20000, "50"},
        {10000, "100"},
        {5000, "200"},
        {2500, "400"},
        {1250, "800"},
        {625, "1600"},
      },
    .powerOnForChange = false,
  };
};

class Smi330LinearAcc : public LinearAcceleration {
public:
  Smi330LinearAcc(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro);
  ~Smi330LinearAcc() = default;
};

class Smi330Gravity : public Gravity {
public:
  Smi330Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro);
  ~Smi330Gravity() = default;
};

class Smi330AccUncalibrated : public Dummy {
public:
  Smi330AccUncalibrated(const std::shared_ptr<SensorCore> base);
  ~Smi330AccUncalibrated() = default;
};

class Smi330GyroUncalibrated : public Dummy {
public:
  Smi330GyroUncalibrated(const std::shared_ptr<SensorCore> base);
  ~Smi330GyroUncalibrated() = default;
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSORS_SMI330_H