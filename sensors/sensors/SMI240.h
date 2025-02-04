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

#ifndef ANDROID_HARDWARE_BOSCH_SENSORS_SMI240_H
#define ANDROID_HARDWARE_BOSCH_SENSORS_SMI240_H

#include "CompositeSensors.h"
#include "SensorCore.h"

namespace bosch {
namespace sensors {

class Smi240Acc : public SensorCore {
public:
  Smi240Acc();
  ~Smi240Acc() = default;
};

class Smi240AccUncalibrated : public Smi240Acc {
public:
  Smi240AccUncalibrated();
  ~Smi240AccUncalibrated() = default;
};

class Smi240Gyro : public SensorCore {
public:
  Smi240Gyro();
  ~Smi240Gyro() = default;
};

class Smi240GyroUncalibrated : public Smi240Gyro {
public:
  Smi240GyroUncalibrated();
  ~Smi240GyroUncalibrated() = default;
};

class Smi240LinearAcc : public LinearAcceleration {
public:
  Smi240LinearAcc(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro);
  ~Smi240LinearAcc() = default;
};

class Smi240Gravity : public Gravity {
public:
  Smi240Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro);
  ~Smi240Gravity() = default;
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSORS_SMI240_H
