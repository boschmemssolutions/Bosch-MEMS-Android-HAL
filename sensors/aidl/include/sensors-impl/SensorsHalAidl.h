/*
 * Copyright (C) 2023 Robert Bosch GmbH
 * Copyright (C) 2023 The Android Open Source Project
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

#pragma once

#include <aidl/android/hardware/common/fmq/SynchronizedReadWrite.h>
#include <aidl/android/hardware/sensors/BnSensors.h>
#include <fmq/AidlMessageQueue.h>
#include <hardware_legacy/power.h>

#include <map>

#include "DirectChannel.h"
#include "Sensor.h"
#include "SensorList.h"

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

using aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::android::AidlMessageQueue;
using ::android::OK;
using ::android::status_t;
using ::android::hardware::EventFlag;

class SensorsHalAidl : public BnSensors, public ISensorsEventCallback {
  static constexpr const char* kWakeLockName = "SensorsHAL_WAKEUP";

public:
  SensorsHalAidl()
    : mEventQueueFlag(nullptr),
      mNextHandle(1),
      mOutstandingWakeUpEvents(0),
      mReadWakeLockQueueRun(false),
      mAutoReleaseWakeLockTime(0),
      mHasWakeLock(false) {
    AddSensors();
  }

  virtual ~SensorsHalAidl() {
    deleteEventFlag();
    mReadWakeLockQueueRun = false;
    mWakeLockThread.join();
  }

  ::ndk::ScopedAStatus activate(int32_t in_sensorHandle, bool in_enabled) override;
  ::ndk::ScopedAStatus batch(int32_t in_sensorHandle, int64_t in_samplingPeriodNs,
                             int64_t in_maxReportLatencyNs) override;
  ::ndk::ScopedAStatus configDirectReport(int32_t in_sensorHandle, int32_t in_channelHandle,
                                          ::aidl::android::hardware::sensors::ISensors::RateLevel in_rate,
                                          int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus flush(int32_t in_sensorHandle) override;
  ::ndk::ScopedAStatus getSensorsList(
    std::vector<::aidl::android::hardware::sensors::SensorInfo>* _aidl_return) override;
  ::ndk::ScopedAStatus initialize(
    const ::aidl::android::hardware::common::fmq::MQDescriptor<
      ::aidl::android::hardware::sensors::Event, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>&
      in_eventQueueDescriptor,
    const ::aidl::android::hardware::common::fmq::MQDescriptor<
      int32_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>& in_wakeLockDescriptor,
    const std::shared_ptr<::aidl::android::hardware::sensors::ISensorsCallback>& in_sensorsCallback) override;
  ::ndk::ScopedAStatus injectSensorData(const ::aidl::android::hardware::sensors::Event& in_event) override;
  ::ndk::ScopedAStatus registerDirectChannel(const ::aidl::android::hardware::sensors::ISensors::SharedMemInfo& in_mem,
                                             int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus setOperationMode(::aidl::android::hardware::sensors::ISensors::OperationMode in_mode) override;
  ::ndk::ScopedAStatus unregisterDirectChannel(int32_t in_channelHandle) override;

  void postEvents(const std::vector<Event>& events, bool wakeup) override {
    std::lock_guard<std::mutex> lock(mWriteLock);
    if (mEventQueue == nullptr) {
      return;
    }
    if (mEventQueue->write(&events.front(), events.size())) {
      mEventQueueFlag->wake(static_cast<uint32_t>(BnSensors::EVENT_QUEUE_FLAG_BITS_READ_AND_PROCESS));

      if (wakeup) {
        // Keep track of the number of outstanding WAKE_UP events in order to
        // properly hold a wake lock until the framework has secured a wake lock
        updateWakeLock(events.size(), 0 /* eventsHandled */);
      }
    }
  }

  void writeToDirectBuffer(const std::vector<Event>& events, int64_t samplingPeriodNs) override {
    if (mChannelMutex.try_lock()) {
      for (const auto& event : events) {
        for (auto& [channelHandle, channel] : mDirectChannels) {
          if (std::find(channel->sensorHandles.begin(), channel->sensorHandles.end(), event.sensorHandle) ==
              channel->sensorHandles.end()) {
            continue;  // Skip channels that sensorHandle not match
          }
          if (channel->rateNs[event.sensorHandle] == 0) {
            continue;  // Skip channels that are not active
          }
          channel->sampleCount[event.sensorHandle]++;
          if (samplingPeriodNs * channel->sampleCount[event.sensorHandle] < channel->rateNs[event.sensorHandle]) {
            continue;  // Skip channels that have faster sampling period
          }
          sensors_event_t ev = {.version = sizeof(sensors_event_t),
                                .sensor = event.sensorHandle,
                                .type = (int32_t)event.sensorType,
                                .reserved0 = 0,
                                .timestamp = event.timestamp};
          if ((event.sensorType == SensorType::GYROSCOPE_UNCALIBRATED) ||
              (event.sensorType == SensorType::ACCELEROMETER_UNCALIBRATED)) {
            ev.uncalibrated_gyro.x_uncalib = event.payload.get<Event::EventPayload::uncal>().x;
            ev.uncalibrated_gyro.y_uncalib = event.payload.get<Event::EventPayload::uncal>().y;
            ev.uncalibrated_gyro.z_uncalib = event.payload.get<Event::EventPayload::uncal>().z;
            ev.uncalibrated_gyro.x_bias = event.payload.get<Event::EventPayload::uncal>().xBias;
            ev.uncalibrated_gyro.y_bias = event.payload.get<Event::EventPayload::uncal>().yBias;
            ev.uncalibrated_gyro.z_bias = event.payload.get<Event::EventPayload::uncal>().zBias;
          } else {
            ev.acceleration.x = event.payload.get<Event::EventPayload::vec3>().x;
            ev.acceleration.y = event.payload.get<Event::EventPayload::vec3>().y;
            ev.acceleration.z = event.payload.get<Event::EventPayload::vec3>().z;
            ev.acceleration.status = (int32_t)event.payload.get<Event::EventPayload::vec3>().status;
          }
          channel->write(&ev);
          channel->sampleCount[event.sensorHandle] = 0;
        }
      }
      mChannelMutex.unlock();
    }
  }

protected:
  // Add new sensors
  void AddSensors();

  // Utility function to delete the Event Flag
  void deleteEventFlag() {
    if (mEventQueueFlag != nullptr) {
      status_t status = EventFlag::deleteEventFlag(&mEventQueueFlag);
      if (status != OK) {
        ALOGI("Failed to delete event flag: %d", status);
      }
    }
  }

  static void startReadWakeLockThread(SensorsHalAidl* sensors) { sensors->readWakeLockFMQ(); }

  // Function to read the Wake Lock FMQ and release the wake lock when
  // appropriate
  void readWakeLockFMQ() {
    while (mReadWakeLockQueueRun.load()) {
      constexpr int64_t kReadTimeoutNs = 500 * 1000 * 1000;  // 500 ms
      int32_t eventsHandled = 0;

      // Read events from the Wake Lock FMQ. Timeout after a reasonable amount
      // of time to ensure that any held wake lock is able to be released if it
      // is held for too long.
      mWakeLockQueue->readBlocking(&eventsHandled, 1 /* count */, 0 /* readNotification */,
                                   static_cast<uint32_t>(WAKE_LOCK_QUEUE_FLAG_BITS_DATA_WRITTEN), kReadTimeoutNs);
      updateWakeLock(0 /* eventsWritten */, eventsHandled);
    }
  }

  /**
   * Responsible for acquiring and releasing a wake lock when there are
   * unhandled WAKE_UP events
   */
  void updateWakeLock(int32_t eventsWritten, int32_t eventsHandled) {
    std::lock_guard<std::mutex> lock(mWakeLockLock);
    int32_t newVal = mOutstandingWakeUpEvents + eventsWritten - eventsHandled;
    if (newVal < 0) {
      mOutstandingWakeUpEvents = 0;
    } else {
      mOutstandingWakeUpEvents = newVal;
    }

    if (eventsWritten > 0) {
      // Update the time at which the last WAKE_UP event was sent
      mAutoReleaseWakeLockTime = ::android::uptimeMillis() + static_cast<uint32_t>(WAKE_LOCK_TIMEOUT_SECONDS) * 1000;
    }

    if (!mHasWakeLock && mOutstandingWakeUpEvents > 0 && acquire_wake_lock(PARTIAL_WAKE_LOCK, kWakeLockName) == 0) {
      mHasWakeLock = true;
    } else if (mHasWakeLock) {
      // Check if the wake lock should be released automatically if
      // SensorTimeout::WAKE_LOCK_SECONDS has elapsed since the last WAKE_UP
      // event was written to the Wake Lock FMQ.
      if (::android::uptimeMillis() > mAutoReleaseWakeLockTime) {
        ALOGD(
          "No events read from wake lock FMQ for %d seconds, auto releasing "
          "wake lock",
          WAKE_LOCK_TIMEOUT_SECONDS);
        mOutstandingWakeUpEvents = 0;
      }

      if (mOutstandingWakeUpEvents == 0 && release_wake_lock(kWakeLockName) == 0) {
        mHasWakeLock = false;
      }
    }
  }

private:
  // The Event FMQ where sensor events are written
  std::unique_ptr<AidlMessageQueue<Event, SynchronizedReadWrite>> mEventQueue;
  // The Wake Lock FMQ that is read to determine when the framework has handled
  // WAKE_UP events
  std::unique_ptr<AidlMessageQueue<int32_t, SynchronizedReadWrite>> mWakeLockQueue;
  // Event Flag to signal to the framework when sensor events are available to
  // be read
  EventFlag* mEventQueueFlag;
  // Callback for asynchronous events, such as dynamic sensor connections.
  std::shared_ptr<::aidl::android::hardware::sensors::ISensorsCallback> mCallback;
  // A map of the report latency in nanoseconds
  std::map<int32_t, int64_t> mReportLatencyNs;
  // A map of the available sensors.
  std::map<int32_t, std::shared_ptr<Sensor>> mSensors;
  // The next available sensor handle
  int32_t mNextHandle;
  // A list of the available sensors
  bosch::sensors::SensorList mSensorList;
  // Lock to protect writes to the FMQs.
  std::mutex mWriteLock;
  // Lock to protect acquiring and releasing the wake lock
  std::mutex mWakeLockLock;
  // Track the number of WAKE_UP events that have not been handled by the
  // framework
  uint32_t mOutstandingWakeUpEvents;
  // A thread to read the Wake Lock FMQ
  std::thread mWakeLockThread;
  // Flag to indicate that the Wake Lock Thread should continue to run
  std::atomic_bool mReadWakeLockQueueRun;
  // Track the time when the wake lock should automatically be released
  int64_t mAutoReleaseWakeLockTime;
  // Flag to indicate if a wake lock has been acquired
  bool mHasWakeLock;

  /**
   * Direct channel support
   */
  std::map<int32_t, std::unique_ptr<::android::DirectChannelBase>> mDirectChannels;
  std::mutex mChannelMutex;
  int32_t mNextChannelHandle = 1;
};

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
