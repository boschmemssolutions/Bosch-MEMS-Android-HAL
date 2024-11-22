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

#ifndef ANDROID_HARDWARE_BOSCH_COMPOSITE_SENSORS_H
#define ANDROID_HARDWARE_BOSCH_COMPOSITE_SENSORS_H

#include <memory>

#include "SensorCore.h"
#include "utils/mat.h"
#include "utils/quat.h"
#include "utils/vec.h"

namespace bosch {
namespace sensors {

class CompositeSensorCore : public ISensorHal {
public:
  CompositeSensorCore() = default;
  ~CompositeSensorCore() override = default;

  void activate(bool enable) override;
  void batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) override;
  bool readSensorTemperature(float* temperature) override;
  const SensorData& getSensorData() const override { return mSensorData; }

  const std::vector<std::shared_ptr<SensorCore>>& getDependencyList() const { return mDependencyList; }

  float mGyroVar = 0;

protected:
  SensorData mSensorData{};
  std::vector<std::shared_ptr<SensorCore>> mDependencyList{};
  SensorValues calculateGravity(std::vector<SensorValues> accValues, std::vector<SensorValues> gyroValues);

private:
  void initRodrParams(const android::vec3_t& acc);
  void predict(const android::vec3_t& w, float dT);
  void update(const android::vec3_t& z, const android::vec3_t& Bi, float sigma);
  void initFusion(const android::vec4_t& q);
  android::mat<float, 3, 4> getF(const android::vec4_t& q);
  void checkState();
  android::quat_t mX0;
  android::vec3_t mX1;
  android::vec3_t mBa, mBm;
  android::mat<android::mat33_t, 2, 2> mPhi;
  android::mat<android::mat33_t, 2, 2> mP;
  android::mat<android::mat33_t, 2, 2> mGQGt;
  int64_t mLastTimestamp{0};
  bool mJustStarted;
  int64_t mSamplingPeriodNs;
};

class LinearAcceleration : public CompositeSensorCore {
public:
  LinearAcceleration() = default;
  ~LinearAcceleration() override = default;

  std::vector<SensorValues> readSensorValues() override;
};

class Gravity : public CompositeSensorCore {
public:
  Gravity() = default;
  ~Gravity() override = default;

  std::vector<SensorValues> readSensorValues() override;
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_COMPOSITE_SENSORS_H
