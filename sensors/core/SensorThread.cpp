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

#include "SensorThread.h"

#include <log/log.h>
#include <poll.h>
#include <utils/SystemClock.h>

#include <iomanip>
#include <sstream>

using namespace bosch::sensors;
using namespace bosch::hwctl;

template <IioDevice Mode>
SensorThread<Mode>::~SensorThread() {
  stop();
}

template <IioDevice Mode>
void SensorThread<Mode>::init(SensorTypeIndex idx, int deviceNum, std::function<void(int64_t)> odrCallback,
                              ScanType scanType, const std::array<float, 9>& rotationMatrix,
                              const std::string& drdyTrigger) {
  std::lock_guard<std::mutex> lock(mRunMutex);

  if (deviceNum != mDeviceNum) {
    mDeviceNum = deviceNum;
    mScanType = scanType;
    mRotationMatrix = rotationMatrix;
    mDrdyTrigger = drdyTrigger;
    mIioDevice = "/sys/bus/iio/devices/iio:device" + std::to_string(deviceNum) + "/";
    mCharDevice = "/dev/iio:device" + std::to_string(deviceNum);

    { WriteReadbackHandler(mIioDevice, "buffer0/enable", "0"); }
    { WriteReadbackHandler(mIioDevice, "buffer0/in_timestamp_en", "1"); }
    { WriteReadbackHandler(mIioDevice, "buffer0/length", std::to_string(kIioBufferLength)); }
    { WriteReadbackHandler(mIioDevice, "buffer0/watermark", "1"); }

    if (mScanType == ScanType::DRDY) {
      switchToDataReadyMode();
    } else if (mScanType == ScanType::HW_FIFO) {
      switchToHwFifoMode();
    } else if (mScanType == ScanType::SW_TIMER) {
      switchToSwTimerMode(idx);
    }
  }

  mOdrCallback[idx] = odrCallback;

  if (idx == ACCEL_IDX) {
    // SMI230 has a separate thread for accel and gyro
    { WriteReadbackHandler(mIioDevice, "buffer0/in_accel_x_en", "1"); }
    { WriteReadbackHandler(mIioDevice, "buffer0/in_accel_y_en", "1"); }
    { WriteReadbackHandler(mIioDevice, "buffer0/in_accel_z_en", "1"); }
  } else if (idx == GYRO_IDX) {
    { WriteReadbackHandler(mIioDevice, "buffer0/in_anglvel_x_en", "1"); }
    { WriteReadbackHandler(mIioDevice, "buffer0/in_anglvel_y_en", "1"); }
    { WriteReadbackHandler(mIioDevice, "buffer0/in_anglvel_z_en", "1"); }
  }
}

template <IioDevice Mode>
void SensorThread<Mode>::switchToDataReadyMode() {
  ALOGD("Switching to data ready mode");

  if (mDrdyTrigger.empty()) {
    ALOGE("Trigger name is empty, cannot switch to data ready mode");
    return;
  }
  WriteReadbackHandler(mIioDevice, "trigger/current_trigger", mDrdyTrigger);
}

template <IioDevice Mode>
void SensorThread<Mode>::switchToHwFifoMode() {
  ALOGD("Switching to hardware fifo mode");

  WriteHandler(mIioDevice, "trigger/current_trigger", "none");

  std::string readback;
  ReadHandler handler(mIioDevice, "trigger/current_trigger");
  const int ret = handler.read(readback);
  if (ret) {
    ALOGE("Failed to read current trigger");
  } else {
    if (!readback.empty()) ALOGE("Failed to switch to hardware fifo mode");
  }
}

template <IioDevice Mode>
void SensorThread<Mode>::switchToSwTimerMode(SensorTypeIndex idx) {
  ALOGD("Switching to software timer mode");

  const std::string trigger = "bosch_hrtimer_trigger" + std::to_string(idx);
  const int ret = createDirectory("/config/iio/triggers/hrtimer/" + trigger);
  if (ret) {
    ALOGE("Failed to create hrtimer trigger directory");
    return;
  }

  const bool triggerExists = isTriggerAvailable(trigger, &mTimerTriggerNum);
  if (!triggerExists) {
    ALOGE("Trigger %s not available", trigger.c_str());
    return;
  }

  WriteReadbackHandler(mIioDevice, "trigger/current_trigger", trigger);
}

template <IioDevice Mode>
void SensorThread<Mode>::setResolution(SensorTypeIndex idx, float resolution) {
  std::lock_guard<std::mutex> lock(mRunMutex);

  if (idx == ACCEL_IDX) {
    mAccResolution = resolution;
  } else if (idx == GYRO_IDX) {
    mGyroResolution = resolution;
  }
}

template <IioDevice Mode>
void SensorThread<Mode>::start() {
  std::lock_guard<std::mutex> lock(mRunMutex);
  if (mRunThread.joinable()) {
    ALOGW("SensorThread[%d]: already running", mDeviceNum);
    return;
  }
  mCurrentTime = android::elapsedRealtimeNano();
  mLastSampleTime = 0;

  mStopThread.store(false);
  mRunThread = std::thread(startThread, this);
}

template <IioDevice Mode>
void SensorThread<Mode>::stop() {
  mStopThread.store(true);

  std::thread threadToJoin;
  {
    std::lock_guard<std::mutex> lock(mRunMutex);
    if (!mRunThread.joinable()) {
      ALOGW("SensorThread[%d]: already stopped", mDeviceNum);
      return;
    }

    threadToJoin = std::move(mRunThread);
  }
  threadToJoin.join();
}

template <IioDevice Mode>
void SensorThread<Mode>::startThread(SensorThread<Mode>* sensor) {
  sensor->run();
}

template <IioDevice Mode>
void SensorThread<Mode>::run() {
  pollfd iioPollfd;
  iioPollfd.fd = open(mCharDevice.c_str(), O_RDONLY | O_NONBLOCK);
  if (iioPollfd.fd < 0) {
    ALOGE("Failed to open iio char device: %s", mCharDevice.c_str());
    return;
  }
  iioPollfd.events = POLLIN;

  updateSamplingRate(false);
  { WriteReadbackHandler(mIioDevice, "buffer0/enable", "1"); }

  while (!mStopThread.load()) {
    bool specialEvent = false;
    const int ret = poll(&iioPollfd, 1, kPollTimeoutMs);

    if (mFlushRequestCount.load() > 0) {
      specialEvent = true;
      readIioBufferAndNotify(iioPollfd.fd);
    }
    if (mSamplingRateChangeRequest.load()) {
      specialEvent = true;
      updateSamplingRate(true);
    }
    if ((ret > 0) && (iioPollfd.revents & POLLIN)) {
      if (!specialEvent) {
        readIioBufferAndNotify(iioPollfd.fd);
      }
    }
  }

  if (iioPollfd.fd >= 0) {
    close(iioPollfd.fd);
    iioPollfd.fd = -1;
  }

  WriteReadbackHandler(mIioDevice, "buffer0/enable", "0");
}

template <IioDevice Mode>
void SensorThread<Mode>::readIioBufferAndNotify(int fd) {
  ssize_t readSize = 0;
  ssize_t combinedReadSize = 0;
  std::array<uint8_t, kMaxLocalBufferSize> buffer{};

  while ((readSize = read(fd, buffer.data(), kMaxLocalBufferSize)) > 0) {
    notify(buffer, readSize, false);
    combinedReadSize += readSize;

    if (combinedReadSize >= (mWatermark * kFrameLength)) {
      break;
    }
    if (readSize == static_cast<ssize_t>(kMaxLocalBufferSize)) {
      usleep(100);  // Yield to other threads
    }
  }

  while (mFlushRequestCount.load() > 0) {
    notify(buffer, 0, true);
  }
}

template <IioDevice Mode>
void SensorThread<Mode>::notify(const std::array<uint8_t, kMaxLocalBufferSize>& buf, int readSize, bool flushRequest) {
  std::vector<SensorValues> accValues{};
  std::vector<SensorValues> gyroValues{};
  std::vector<std::weak_ptr<ISensorValues>> accObserversCopy;
  std::vector<std::weak_ptr<ISensorValues>> gyroObserversCopy;
  {
    std::lock_guard<std::mutex> lock(mRunMutex);
    for (int i = 0; i < readSize / kFrameLength; i++) {
      const int idx = i * kFrameLength;
      const int64_t timestamp = *(int64_t*)(buf.data() + kTimestampIdx + idx);

      int64_t delta = mHwSamplingPeriodNs;
      if ((mLastSampleTime != 0) && (timestamp > mLastSampleTime)) {
        delta = timestamp - mLastSampleTime;
        if (delta > 10 * mHwSamplingPeriodNs) {  // Clamp delta
          delta = mHwSamplingPeriodNs;
        }
      }

      if (mIsEnabled[ACCEL_IDX]) {
        SensorValues value{};
        value.type = BoschSensorType::ACCEL;
        value.timestamp = mCurrentTime;
        const int16_t x = static_cast<int16_t>((buf.at(kAccXIdx + idx + 1) << 8) | buf.at(kAccXIdx + idx));
        const int16_t y = static_cast<int16_t>((buf.at(kAccYIdx + idx + 1) << 8) | buf.at(kAccYIdx + idx));
        const int16_t z = static_cast<int16_t>((buf.at(kAccZIdx + idx + 1) << 8) | buf.at(kAccZIdx + idx));
        const float x_rot = mRotationMatrix[0] * x + mRotationMatrix[1] * y + mRotationMatrix[2] * z;
        const float y_rot = mRotationMatrix[3] * x + mRotationMatrix[4] * y + mRotationMatrix[5] * z;
        const float z_rot = mRotationMatrix[6] * x + mRotationMatrix[7] * y + mRotationMatrix[8] * z;
        value.data.push_back(x_rot * mAccResolution);
        value.data.push_back(y_rot * mAccResolution);
        value.data.push_back(z_rot * mAccResolution);
        accValues.push_back(value);
      }

      if (mIsEnabled[GYRO_IDX]) {
        SensorValues value{};
        value.type = BoschSensorType::GYRO;
        value.timestamp = mCurrentTime;
        const int16_t x = static_cast<int16_t>((buf.at(kGyroXIdx + idx + 1) << 8) | buf.at(kGyroXIdx + idx));
        const int16_t y = static_cast<int16_t>((buf.at(kGyroYIdx + idx + 1) << 8) | buf.at(kGyroYIdx + idx));
        const int16_t z = static_cast<int16_t>((buf.at(kGyroZIdx + idx + 1) << 8) | buf.at(kGyroZIdx + idx));
        const float x_rot = mRotationMatrix[0] * x + mRotationMatrix[1] * y + mRotationMatrix[2] * z;
        const float y_rot = mRotationMatrix[3] * x + mRotationMatrix[4] * y + mRotationMatrix[5] * z;
        const float z_rot = mRotationMatrix[6] * x + mRotationMatrix[7] * y + mRotationMatrix[8] * z;
        value.data.push_back(x_rot * mGyroResolution);
        value.data.push_back(y_rot * mGyroResolution);
        value.data.push_back(z_rot * mGyroResolution);
        gyroValues.push_back(value);
      }

      mLastSampleTime = timestamp;
      mCurrentTime += delta;
    }

    if (flushRequest) {
      if (mIsEnabled[ACCEL_IDX]) {
        SensorValues value{};
        value.type = BoschSensorType::ACCEL;
        value.flushRequest = true;
        accValues.push_back(value);
      }
      if (mIsEnabled[GYRO_IDX]) {
        SensorValues value{};
        value.type = BoschSensorType::GYRO;
        value.flushRequest = true;
        gyroValues.push_back(value);
      }
      mFlushRequestCount.fetch_sub(1);
    }

    cleanAndCopyObservers(ACCEL_IDX, accObserversCopy);
    cleanAndCopyObservers(GYRO_IDX, gyroObserversCopy);
  }

  if (!accValues.empty()) {
    for (const auto& obs : accObserversCopy) {
      if (auto observerPtr = obs.lock()) {
        observerPtr->update(accValues);
      }
    }
  }
  if (!gyroValues.empty()) {
    for (const auto& obs : gyroObserversCopy) {
      if (auto observerPtr = obs.lock()) {
        observerPtr->update(gyroValues);
      }
    }
  }
}

template <IioDevice Mode>
void SensorThread<Mode>::flush() {
  mFlushRequestCount.fetch_add(1);
}

template <IioDevice Mode>
void SensorThread<Mode>::registerObserver(SensorTypeIndex idx, const std::shared_ptr<ISensorValues>& observer) {
  std::lock_guard<std::mutex> lock(mRunMutex);
  mObservers[idx].push_back(observer);
}

template <IioDevice Mode>
void SensorThread<Mode>::removeObserver(SensorTypeIndex idx, const std::shared_ptr<ISensorValues>& observer) {
  std::lock_guard<std::mutex> lock(mRunMutex);
  mObservers[idx].erase(
    std::remove_if(mObservers[idx].begin(), mObservers[idx].end(),
                   [observer](const std::weak_ptr<ISensorValues>& obs) { return obs.lock() == observer; }),
    mObservers[idx].end());
}

template <IioDevice Mode>
void SensorThread<Mode>::cleanAndCopyObservers(SensorTypeIndex idx,
                                               std::vector<std::weak_ptr<ISensorValues>>& observersCopy) {
  mObservers[idx].erase(std::remove_if(mObservers[idx].begin(), mObservers[idx].end(),
                                       [&observersCopy](const std::weak_ptr<ISensorValues>& obs) {
                                         std::shared_ptr<ISensorValues> sharedObs = obs.lock();
                                         if (sharedObs) {
                                           observersCopy.push_back(obs);
                                           return false;
                                         }
                                         return true;
                                       }),
                        mObservers[idx].end());
}

template <IioDevice Mode>
void SensorThread<Mode>::enable(SensorTypeIndex idx, bool enable, const std::string& sysfsPwr,
                                const std::string& mode) {
  bool wasAnyEnabled = false;
  bool isAnyEnabled = false;
  {
    std::lock_guard<std::mutex> lock(mRunMutex);
    if (enable == mIsEnabled[idx]) {
      return;  // nothing changed
    }
    wasAnyEnabled = mIsEnabled[ACCEL_IDX] || mIsEnabled[GYRO_IDX];
    mIsEnabled[idx] = enable;
    isAnyEnabled = mIsEnabled[ACCEL_IDX] || mIsEnabled[GYRO_IDX];
  }

  if (enable) {
    setPowerMode(sysfsPwr, mode);
  }

  mSamplingRateChangeRequest.store(true);

  if (enable && !wasAnyEnabled) {
    start();
  } else if (!isAnyEnabled) {
    stop();
  }

  if (!enable) {
    setPowerMode(sysfsPwr, mode);
  }
}

template <IioDevice Mode>
void SensorThread<Mode>::setPowerMode(const std::string& sysfsPwr, const std::string& mode) {
  if (!sysfsPwr.empty() && !mode.empty()) {
    { WriteHandler(mIioDevice, sysfsPwr, mode); }
    usleep(100000);

    std::string readback;
    ReadHandler handler(mIioDevice, sysfsPwr);
    const int ret = handler.read(readback);
    if (ret || (readback != mode)) ALOGE("Failed to set power mode %s to %s", mode.c_str(), sysfsPwr.c_str());
  }
}

template <IioDevice Mode>
void SensorThread<Mode>::setSamplingRate(SensorTypeIndex idx, int64_t samplingPeriodNs, int64_t reportLatencyNs) {
  std::lock_guard<std::mutex> lock(mRunMutex);
  mSamplingPeriodNs[idx] = samplingPeriodNs;
  mReportLatencyNs[idx] = reportLatencyNs;
  mSamplingRateChangeRequest.store(true);
}

template <IioDevice Mode>
void SensorThread<Mode>::updateSamplingRate(bool isRunning) {
  int64_t usedHwSamplingPeriodNs = INT64_MAX;
  int64_t usedReportLatencyNs = INT64_MAX;
  size_t watermark = 1;
  std::array<std::function<void(int64_t)>, IDX_LENGTH> callbacks;
  std::array<bool, IDX_LENGTH> enabledCopy;

  {
    std::lock_guard<std::mutex> lock(mRunMutex);

    if (!mIsEnabled[ACCEL_IDX] && !mIsEnabled[GYRO_IDX]) {
      return;
    }

    mSamplingRateChangeRequest.store(false);

    for (size_t idx = 0; idx < IDX_LENGTH; idx++) {
      if (mIsEnabled[idx]) {
        usedHwSamplingPeriodNs = std::min(usedHwSamplingPeriodNs, mSamplingPeriodNs[idx]);
        usedReportLatencyNs = std::min(usedReportLatencyNs, mReportLatencyNs[idx]);
      }
    }

    watermark =
      std::max(1ul, std::min(kIioBufferLength / 2, static_cast<size_t>(usedReportLatencyNs / usedHwSamplingPeriodNs)));

    mHwSamplingPeriodNs = usedHwSamplingPeriodNs;
    mWatermark = watermark;
    mCurrentTime = android::elapsedRealtimeNano();
    mLastSampleTime = 0;

    callbacks = mOdrCallback;
    enabledCopy = mIsEnabled;
  }

  if (isRunning) {
    WriteReadbackHandler(mIioDevice, "buffer0/enable", "0");
  }

  for (size_t idx = 0; idx < IDX_LENGTH; idx++) {
    if (enabledCopy[idx] && callbacks[idx]) {
      callbacks[idx](usedHwSamplingPeriodNs);
    }
  }

  {
    ALOGD("Setting watermark to %ld", watermark);
    WriteReadbackHandler(mIioDevice, "buffer0/watermark", std::to_string(watermark));
  }

  if (mScanType == ScanType::HW_FIFO) {
    if (watermark > kOpModeChangeWmLimit) {
      switchToHwFifoMode();
    } else {
      switchToDataReadyMode();
    }
  } else if (mScanType == ScanType::SW_TIMER) {
    const int64_t frequency = (1e9 + usedHwSamplingPeriodNs - 1) / usedHwSamplingPeriodNs;
    const std::string sysfsPath = "/sys/bus/iio/devices/trigger" + std::to_string(mTimerTriggerNum) + "/";

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << static_cast<double>(frequency);
    WriteReadbackHandler(sysfsPath, "sampling_frequency", oss.str());
  }

  if (isRunning) {
    WriteReadbackHandler(mIioDevice, "buffer0/enable", "1");
  }
}

template class bosch::sensors::SensorThread<IioDevice::ACC_AND_GYRO_COMBINED>;
template class bosch::sensors::SensorThread<IioDevice::ACC_AND_GYRO_SEPARATE>;
