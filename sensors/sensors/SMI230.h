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

#ifndef ANDROID_HARDWARE_BOSCH_SENSORS_SMI230_H
#define ANDROID_HARDWARE_BOSCH_SENSORS_SMI230_H

#include "Accelerometer.h"
#include "CompositeSensors.h"
#include "Gyroscope.h"

namespace bosch {
namespace sensors {

class Smi230Acc : public Accelerometer {
public:
  Smi230Acc(const std::shared_ptr<ISensorThread> sensorThread);
  ~Smi230Acc() = default;

private:
  const AccelData mAccelData = {
    .sysfsEnable = "1",
    .sysfsDisable = "0",
    .sysfsTrigger = "smi230acc-trigger",
    .defaultRange = AccelRange::RANGE_4G,
    .minRange = AccelRange::RANGE_2G,
    .maxRange = AccelRange::RANGE_16G,
    .odrMap =
      {
        {80000, "12.500000000"},
        {40000, "25.000000000"},
        {20000, "50.000000000"},
        {10000, "100.000000000"},
        {5000, "200.000000000"},
        {2500, "400.000000000"},
        {1250, "800.000000000"},
        {625, "1600.000000000"},
      },
  };
};

class Smi230Gyro : public Gyroscope {
public:
  Smi230Gyro(const std::shared_ptr<ISensorThread> sensorThread);
  ~Smi230Gyro() = default;

private:
  const GyroData mGyroData = {
    .sysfsEnable = "1",
    .sysfsDisable = "0",
    .sysfsTrigger = "smi230gyro-trigger",
    .defaultRange = GyroRange::RANGE_2000_DEG_S,
    .minRange = GyroRange::RANGE_125_DEG_S,
    .maxRange = GyroRange::RANGE_2000_DEG_S,
    .odrMap =
      {
        {10000, "101"},
        {5000, "201"},
        {2500, "400"},
        {1000, "1000"},
      },
    .powerOnForChange = true,
  };
};

class Smi230LinearAcc : public LinearAcceleration {
public:
  Smi230LinearAcc(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro);
  ~Smi230LinearAcc() = default;
};

class Smi230Gravity : public Gravity {
public:
  Smi230Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro);
  ~Smi230Gravity() = default;
};

class Smi230AccUncalibrated : public Dummy {
public:
  Smi230AccUncalibrated(const std::shared_ptr<SensorCore> base);
  ~Smi230AccUncalibrated() = default;
};

class Smi230GyroUncalibrated : public Dummy {
public:
  Smi230GyroUncalibrated(const std::shared_ptr<SensorCore> base);
  ~Smi230GyroUncalibrated() = default;
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSORS_SMI230_H