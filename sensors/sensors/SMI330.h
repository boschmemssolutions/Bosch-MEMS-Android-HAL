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

#ifndef ANDROID_HARDWARE_BOSCH_SENSORS_SMI330_H
#define ANDROID_HARDWARE_BOSCH_SENSORS_SMI330_H

#include <array>

#include "CompositeSensors.h"
#include "SensorCore.h"

namespace bosch {
namespace sensors {

class Smi330Imu {
public:
  Smi330Imu(const Smi330Imu&) = delete;
  Smi330Imu& operator=(const Smi330Imu&) = delete;

  static Smi330Imu& getInstance() {
    static Smi330Imu instance;
    return instance;
  }

  enum Index { ACCEL, GYRO, LENGTH };

  void setPowerMode(Index idx, bool enable, const std::string& device);
  void setSamplingRate(Index idx, int64_t samplingPeriodNs, const std::string& device);

private:
  Smi330Imu() = default;
  ~Smi330Imu() = default;

  void updateSamplingRate(const std::string& device);

  const int64_t mMinSamplingRateNs = 2500000;
  const int64_t mMaxSamplingRateNs = 1280000000;
  const std::string mSysfsOdr{"in_sampling_frequency"};
  const std::array<std::string, Index::LENGTH> mSysfsPowerMode{"in_accel_en", "in_anglvel_en"};

  std::array<bool, Index::LENGTH> mIsEnabled{false, false};
  std::array<int64_t, Index::LENGTH> mSamplingPeriodNs{mMaxSamplingRateNs, mMaxSamplingRateNs};
};

class Smi330Acc : public SensorCore {
public:
  Smi330Acc();
  ~Smi330Acc() = default;

  void setPowerMode(bool enable) override {
    Smi330Imu::getInstance().setPowerMode(Smi330Imu::Index::ACCEL, enable, mDevice);
  };
  void setSamplingRate(int64_t samplingPeriodNs) override {
    Smi330Imu::getInstance().setSamplingRate(Smi330Imu::Index::ACCEL, samplingPeriodNs, mDevice);
  };
};

class Smi330AccUncalibrated : public Smi330Acc {
public:
  Smi330AccUncalibrated();
  ~Smi330AccUncalibrated() = default;
};

class Smi330Gyro : public SensorCore {
public:
  Smi330Gyro();
  ~Smi330Gyro() = default;

  void setPowerMode(bool enable) override {
    if (enable) setScale();
    Smi330Imu::getInstance().setPowerMode(Smi330Imu::Index::GYRO, enable, mDevice);
  };
  void setSamplingRate(int64_t samplingPeriodNs) override {
    Smi330Imu::getInstance().setSamplingRate(Smi330Imu::Index::GYRO, samplingPeriodNs, mDevice);
  };

private:
  void setScale();

  const std::string mSysfsScale{"in_anglvel_scale"};
};

class Smi330GyroUncalibrated : public Smi330Gyro {
public:
  Smi330GyroUncalibrated();
  ~Smi330GyroUncalibrated() = default;
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

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSORS_SMI330_H