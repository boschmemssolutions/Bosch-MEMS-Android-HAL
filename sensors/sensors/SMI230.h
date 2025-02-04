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

#ifndef ANDROID_HARDWARE_BOSCH_SENSORS_SMI230_H
#define ANDROID_HARDWARE_BOSCH_SENSORS_SMI230_H

#include "CompositeSensors.h"
#include "SensorCore.h"

namespace bosch {
namespace sensors {

class Smi230Acc : public SensorCore {
public:
  Smi230Acc();
  ~Smi230Acc() = default;

  void setPowerMode(bool enable) override;

private:
  const std::string mSysfsPowerMode = "pwr";
  const std::string mSysfsOdr = "odr";
};

class Smi230AccUncalibrated : public Smi230Acc {
public:
  Smi230AccUncalibrated();
  ~Smi230AccUncalibrated() = default;
};

class Smi230Gyro : public SensorCore {
public:
  Smi230Gyro();
  ~Smi230Gyro() = default;

  void setPowerMode(bool enable) override;

private:
  const std::string mSysfsPowerMode = "pwr";
  const std::string mSysfsOdr = "bw_odr";
};

class Smi230GyroUncalibrated : public Smi230Gyro {
public:
  Smi230GyroUncalibrated();
  ~Smi230GyroUncalibrated() = default;
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

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSORS_SMI230_H