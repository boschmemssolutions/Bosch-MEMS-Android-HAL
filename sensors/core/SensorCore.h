/*
 * Copyright (C) 2024-2026 Robert Bosch GmbH
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

#ifndef ANDROID_HARDWARE_BOSCH_SENSOR_CORE_H
#define ANDROID_HARDWARE_BOSCH_SENSOR_CORE_H

#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "FileHandler.h"
#include "ISensorHal.h"
#include "ISensorThread.h"

namespace bosch {
namespace sensors {

constexpr float gravityToAcceleration(float gravity) { return gravity * 9.80665f; }
constexpr float degreeToRad(float degree) { return degree * M_PI / 180.0f; }
constexpr int64_t us_ns(int64_t us) { return us * 1000; }

class SensorCore : public ISensorHal, public ISensorValues, public std::enable_shared_from_this<SensorCore> {
public:
  SensorCore(const std::shared_ptr<ISensorThread> sensorThread);
  ~SensorCore() override;

  void registerObserver(const std::shared_ptr<ISensorValues>& observer) override;
  void removeObserver(const std::shared_ptr<ISensorValues>& observer) override;
  bool readSensorTemperature(float* temperature) override;
  void activate(bool enable) override;
  void batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) override;
  void flush() override;
  const SensorData& getSensorData() const override { return mSensorData; }
  const std::array<float, 9>& getRotationMatrix() const override { return mRotationMatrix; }
  const std::array<float, 3>& getLocationVector() const override { return mLocationVector; }

  void update(const std::vector<SensorValues>& values) override;

  virtual void init(int deviceNum) = 0;
  bool isAvailable() const { return mAvailable; }

  void registerCompositeObserver(const std::shared_ptr<ICompositeObserverCb>& observer);
  void removeCompositeObserver(const std::shared_ptr<ICompositeObserverCb>& observer);

  void activateByType(BoschSensorType type, bool enable);
  void batchByType(BoschSensorType type, int64_t samplingPeriodNs, int64_t maxReportLatencyNs);

  void updateValidTimestampFactor(int64_t odrNs);

protected:
  void initBase(int deviceNum);
  virtual void enableSensor(bool enable) = 0;
  virtual void setSamplingRate(int64_t samplingPeriodNs, int64_t reportLatencyNs) = 0;
  ScanType readScanType(ScanType defaultScanType);
  AccelRange readAccelRange(AccelRange defaultRange);
  GyroRange readGyroRange(GyroRange defaultRange);

  std::string mDevice{};
  SensorData mSensorData{};

private:
  void readRotationMatrix();
  void readLocationVector();
  void updateSamplingRate();
  void cleanAndCopyObservers(std::vector<std::weak_ptr<ISensorValues>>& observersCopy);
  void cleanAndCopyCompositeObservers(std::vector<std::weak_ptr<ICompositeObserverCb>>& observersCopy);
  void run();
  static void startThread(SensorCore* sensor);

  bool mAvailable{false};
  bool mIsEnabled{false};
  std::map<BoschSensorType, bool> mEnableState{};
  std::map<BoschSensorType, int64_t> mSamplingPeriods{};
  std::map<BoschSensorType, int64_t> mReportLatency{};

  std::atomic_bool mStopThread{false};
  std::condition_variable mWaitCV;
  std::thread mRunThread;
  std::mutex mRunMutex;
  std::vector<std::weak_ptr<ISensorValues>> mObservers;
  std::vector<std::weak_ptr<ICompositeObserverCb>> mCompositeObservers;

  int64_t mLastTimestampNs{0};
  int64_t mMinValidDiffNs{0};

  std::vector<SensorValues> mSensorValues;
  const std::shared_ptr<ISensorThread> mSensorThread;

  std::array<float, 9> mRotationMatrix{0};
  std::array<float, 3> mLocationVector{0};
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSOR_CORE_H
