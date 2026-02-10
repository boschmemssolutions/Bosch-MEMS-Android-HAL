/*
 * Copyright (C) 2024,2026 Robert Bosch GmbH
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

#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "SensorCore.h"
#include "utils/mat.h"
#include "utils/quat.h"
#include "utils/vec.h"

namespace bosch {
namespace sensors {

class CompositeSensorCore : public ISensorHal, public ICompositeObserverCb {
public:
  CompositeSensorCore() = default;
  ~CompositeSensorCore() override = default;

  void registerObserver(const std::shared_ptr<ISensorValues>& observer) override;
  void removeObserver(const std::shared_ptr<ISensorValues>& observer) override;
  void batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) override;
  void flush() override;
  bool readSensorTemperature(float* temperature) override;
  const SensorData& getSensorData() const override { return mSensorData; }
  const std::array<float, 9>& getRotationMatrix() const override { return mDependencyList[0]->getRotationMatrix(); };
  const std::array<float, 3>& getLocationVector() const override { return mDependencyList[0]->getLocationVector(); };

  void updateValidTimestampFactor(int64_t odrNs) override;

  const std::vector<std::shared_ptr<SensorCore>>& getDependencyList() const { return mDependencyList; }

  void init();

protected:
  void cleanAndCopyObservers(std::vector<std::weak_ptr<ISensorValues>>& observersCopy);

  SensorData mSensorData{};
  std::vector<std::shared_ptr<SensorCore>> mDependencyList{};
  std::vector<std::weak_ptr<ISensorValues>> mObservers;
  std::mutex mMutex;

  bool mJustStarted{false};
  int64_t mSamplingPeriodNs;

  std::atomic_bool mStopThread{false};
  std::condition_variable mWaitCV;
  std::thread mRunThread;

  int64_t mLastAccTimestampNs{0};
  int64_t mLastGyroTimestampNs{0};
  int64_t mLastTimestampNs{0};
  int64_t mMinValidDiffNs{0};
};

class CompositeSensor : public CompositeSensorCore, public std::enable_shared_from_this<CompositeSensor> {
public:
  CompositeSensor(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro,
                  std::function<SensorValues(const SensorValues&, const SensorValues&)> calcCallback);
  ~CompositeSensor() override;

  void activate(bool enable) override;
  void update(const std::vector<SensorValues>& values) override;

protected:
  SensorValues calculateGravity(const SensorValues& accValues, const SensorValues& gyroValues);

  static constexpr size_t MAX_QUEUE_SIZE = 1024;

  float mGyroVar{0};
  int64_t mAccelFlushRequestCount{0};
  int64_t mGyroFlushRequestCount{0};

private:
  static void startThread(CompositeSensor* sensor);

  void run();
  void initRodrParams(const android::vec3_t& acc);
  void predict(const android::vec3_t& w, float dT);
  void updateFusion(const android::vec3_t& z, const android::vec3_t& Bi, float sigma);
  void initFusion(const android::vec4_t& q);
  void checkState();
  android::mat<float, 3, 4> getF(const android::vec4_t& q);

  std::function<SensorValues(const SensorValues&, const SensorValues&)> mCalcCallback;

  std::deque<SensorValues> mAccValues;
  std::deque<SensorValues> mGyroValues;
  std::deque<SensorValues> mMetaValues;

  int64_t mLastTimestamp{0};
  android::quat_t mX0;
  android::vec3_t mX1;
  android::vec3_t mBa, mBm;
  android::mat<android::mat33_t, 2, 2> mPhi;
  android::mat<android::mat33_t, 2, 2> mP;
  android::mat<android::mat33_t, 2, 2> mGQGt;
};

class LinearAcceleration : public CompositeSensor {
public:
  LinearAcceleration(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro);
  ~LinearAcceleration() override = default;

  SensorValues calc(const SensorValues& accValues, const SensorValues& gyroValues);
};

class Gravity : public CompositeSensor {
public:
  Gravity(const std::shared_ptr<SensorCore> accel, const std::shared_ptr<SensorCore> gyro);
  ~Gravity() override = default;

  SensorValues calc(const SensorValues& accValues, const SensorValues& gyroValues);
};

class Dummy : public CompositeSensorCore, public std::enable_shared_from_this<Dummy> {
public:
  Dummy(const std::shared_ptr<SensorCore> base);
  ~Dummy() override;

  void activate(bool enable) override;
  void update(const std::vector<SensorValues>& values) override;

private:
  void run();
  static void startThread(Dummy* sensor);

  std::vector<SensorValues> mValues;
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_COMPOSITE_SENSORS_H
