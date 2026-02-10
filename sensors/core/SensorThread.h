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

#ifndef ANDROID_HARDWARE_BOSCH_SENSOR_THREAD_H
#define ANDROID_HARDWARE_BOSCH_SENSOR_THREAD_H

#include <array>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "FileHandler.h"
#include "ISensorHal.h"
#include "ISensorThread.h"

namespace bosch {
namespace sensors {

enum class IioDevice { ACC_AND_GYRO_COMBINED, ACC_AND_GYRO_SEPARATE };

template <IioDevice Mode>
class SensorThread : public ISensorThread {
public:
  SensorThread() = default;
  ~SensorThread() override;

  void init(SensorTypeIndex idx, int deviceNum, std::function<void(int64_t)> odrCallback, ScanType scanType,
            const std::array<float, 9>& rotationMatrix, const std::string& drdyTrigger = "") override;
  void setResolution(SensorTypeIndex idx, float resolution) override;

  void registerObserver(SensorTypeIndex idx, const std::shared_ptr<ISensorValues>& observer) override;
  void removeObserver(SensorTypeIndex idx, const std::shared_ptr<ISensorValues>& observer) override;

  void enable(SensorTypeIndex idx, bool enable, const std::string& sysfsPwr, const std::string& mode) override;
  void setSamplingRate(SensorTypeIndex idx, int64_t samplingPeriodNs, int64_t reportLatencyNs) override;

  void flush() override;

private:
  static constexpr size_t kOpModeChangeWmLimit = 5;
  static constexpr size_t kPollTimeoutMs = 200;

  static constexpr int kAccXIdx = Mode == IioDevice::ACC_AND_GYRO_COMBINED ? 0 : 0;
  static constexpr int kAccYIdx = Mode == IioDevice::ACC_AND_GYRO_COMBINED ? 2 : 2;
  static constexpr int kAccZIdx = Mode == IioDevice::ACC_AND_GYRO_COMBINED ? 4 : 4;
  static constexpr int kGyroXIdx = Mode == IioDevice::ACC_AND_GYRO_COMBINED ? 6 : 0;
  static constexpr int kGyroYIdx = Mode == IioDevice::ACC_AND_GYRO_COMBINED ? 8 : 2;
  static constexpr int kGyroZIdx = Mode == IioDevice::ACC_AND_GYRO_COMBINED ? 10 : 4;
  static constexpr int kTimestampIdx = Mode == IioDevice::ACC_AND_GYRO_COMBINED ? 16 : 8;
  static constexpr int kFrameLength = Mode == IioDevice::ACC_AND_GYRO_COMBINED ? 24 : 16;

  static constexpr size_t kMaxLocalFrames = 32;
  static constexpr size_t kIioBufferLength = 32 * kMaxLocalFrames;
  static constexpr size_t kMaxLocalBufferSize = kMaxLocalFrames * kFrameLength;

  static void startThread(SensorThread* sensor);

  void run();
  void start();
  void stop();

  void switchToDataReadyMode();
  void switchToHwFifoMode();
  void switchToSwTimerMode(SensorTypeIndex idx);
  void setPowerMode(const std::string& sysfsPwr, const std::string& mode);
  void updateSamplingRate(bool isRunning);
  void readIioBufferAndNotify(int fd);
  void notify(const std::array<uint8_t, kMaxLocalBufferSize>& buf, int readSize, bool flushRequest);
  void cleanAndCopyObservers(SensorTypeIndex idx, std::vector<std::weak_ptr<ISensorValues>>& observersCopy);

  enum ScanType mScanType;
  int mDeviceNum{-1};
  int mTimerTriggerNum{-1};
  float mAccResolution{1};
  float mGyroResolution{1};
  std::string mIioDevice{};
  std::string mCharDevice{};
  std::string mDrdyTrigger{};
  std::array<std::function<void(int64_t)>, IDX_LENGTH> mOdrCallback{};
  std::array<std::vector<std::weak_ptr<ISensorValues>>, IDX_LENGTH> mObservers{};
  std::array<float, 9> mRotationMatrix{};

  std::array<bool, IDX_LENGTH> mIsEnabled{false, false};
  std::array<int64_t, IDX_LENGTH> mSamplingPeriodNs{0, 0};
  std::array<int64_t, IDX_LENGTH> mReportLatencyNs{0, 0};
  int64_t mHwSamplingPeriodNs{0};
  int64_t mLastSampleTime{0};
  int64_t mCurrentTime{0};
  int64_t mWatermark{1};

  std::atomic_size_t mFlushRequestCount{0};

  std::atomic_bool mStopThread{false};
  std::atomic_bool mSamplingRateChangeRequest{false};
  std::mutex mRunMutex;
  std::thread mRunThread;
};

using SensorThreadCombined = SensorThread<IioDevice::ACC_AND_GYRO_COMBINED>;
using SensorThreadSeparate = SensorThread<IioDevice::ACC_AND_GYRO_SEPARATE>;

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSOR_THREAD_H
