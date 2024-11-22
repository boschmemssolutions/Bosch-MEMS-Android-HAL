/*
 * Copyright (C) 2023 Robert Bosch GmbH. All rights reserved.
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

#ifndef ANDROID_HARDWARE_SENSORS_V2_X_SENSORS_H
#define ANDROID_HARDWARE_SENSORS_V2_X_SENSORS_H

#include <android/hardware/sensors/2.0/ISensors.h>
#include <android/hardware/sensors/2.0/types.h>
#include <fmq/MessageQueue.h>
#include <hardware_legacy/power.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <log/log.h>

#include <atomic>
#include <memory>
#include <thread>

#include "DirectChannel.h"
#include "EventMessageQueueWrapper.h"
#include "Sensor.h"
#include "SensorList.h"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_X {
namespace implementation {

using Configuration = ::bosch::sensor::hal::configuration::V1_0::Configuration;
using SensorType = ::android::hardware::sensors::V2_1::SensorType;
using RateLevel = ::android::hardware::sensors::V1_0::RateLevel;

#define SENSOR_XML_CONFIG_FILE_NAME "sensor_hal_configuration.xml"
static const char* gSensorConfigLocationList[] = {"/odm/etc/sensors/", "/vendor/etc/sensors/"};
static const int gSensorConfigLocationListSize =
  (sizeof(gSensorConfigLocationList) / sizeof(gSensorConfigLocationList[0]));

#define MODULE_NAME "bosch-hal"

static inline std::optional<std::vector<Configuration>> getSensorConfiguration(
  const std::vector<::bosch::sensor::hal::configuration::V1_0::Sensor>& sensor_list, const std::string& name,
  SensorType type) {
  for (auto sensor : sensor_list) {
    if ((name.compare(sensor.getName()) == 0) && (type == (SensorType)sensor.getType())) {
      return sensor.getConfiguration();
    }
  }
  return std::nullopt;
}

static inline std::optional<std::vector<::bosch::sensor::hal::configuration::V1_0::Sensor>> readSensorsConfigFromXml() {
  for (int i = 0; i < gSensorConfigLocationListSize; i++) {
    const auto sensor_config_file = std::string(gSensorConfigLocationList[i]) + SENSOR_XML_CONFIG_FILE_NAME;
    auto sensorConfig = ::bosch::sensor::hal::configuration::V1_0::read(sensor_config_file.c_str());
    if (sensorConfig) {
      auto modulesList = sensorConfig->getFirstModules()->get_module();
      for (auto module : modulesList) {
        if (module.getHalName().compare(MODULE_NAME) == 0) {
          return module.getFirstSensors()->getSensor();
        }
      }
    }
  }
  return std::nullopt;
}

template <class ISensorsInterface>
struct Sensors : public ISensorsInterface, public ISensorsEventCallback {
  using Event = ::android::hardware::sensors::V1_0::Event;
  using OperationMode = ::android::hardware::sensors::V1_0::OperationMode;
  using RateLevel = ::android::hardware::sensors::V1_0::RateLevel;
  using Result = ::android::hardware::sensors::V1_0::Result;
  using SharedMemInfo = ::android::hardware::sensors::V1_0::SharedMemInfo;
  using EventQueueFlagBits = ::android::hardware::sensors::V2_0::EventQueueFlagBits;
  using SensorTimeout = ::android::hardware::sensors::V2_0::SensorTimeout;
  using WakeLockQueueFlagBits = ::android::hardware::sensors::V2_0::WakeLockQueueFlagBits;
  using ISensorsCallback = ::android::hardware::sensors::V2_0::ISensorsCallback;
  using SensorInfo = ::android::hardware::sensors::V2_1::SensorInfo;
  using EventMessageQueue = MessageQueue<Event, kSynchronizedReadWrite>;
  using WakeLockMessageQueue = MessageQueue<uint32_t, kSynchronizedReadWrite>;

  static constexpr const char* kWakeLockName = "SensorsHAL_WAKEUP";

  Sensors()
    : mEventQueueFlag(nullptr),
      mNextHandle(1),
      mOutstandingWakeUpEvents(0),
      mReadWakeLockQueueRun(false),
      mAutoReleaseWakeLockTime(0),
      mHasWakeLock(false) {
    AddSensors();
  }

  virtual ~Sensors() {
    deleteEventFlag();
    mReadWakeLockQueueRun = false;
    mWakeLockThread.join();
  }

  // Methods from ::android::hardware::sensors::V2_0::ISensors follow.
  Return<Result> setOperationMode(OperationMode mode) override {
    Result res = Result::OK;
    for (auto sensor : mSensors) {
      res = sensor.second->setOperationMode(mode);
    }
    return res;
  }

  Return<Result> activate(int32_t sensorHandle, bool enabled) override {
    auto sensor = mSensors.find(sensorHandle);
    if (sensor != mSensors.end()) {
      sensor->second->activate(enabled);
      return Result::OK;
    }
    return Result::BAD_VALUE;
  }

  Return<Result> initialize(const ::android::hardware::MQDescriptorSync<Event>& eventQueueDescriptor,
                            const ::android::hardware::MQDescriptorSync<uint32_t>& wakeLockDescriptor,
                            const sp<ISensorsCallback>& sensorsCallback) override {
    auto eventQueue = std::make_unique<EventMessageQueue>(eventQueueDescriptor, true /* resetPointers */);
    std::unique_ptr<V2_1::implementation::EventMessageQueueWrapperBase> wrapper =
      std::make_unique<V2_1::implementation::EventMessageQueueWrapperV1_0>(eventQueue);
    return initializeBase(wrapper, wakeLockDescriptor, sensorsCallback);
  }

  Return<Result> initializeBase(std::unique_ptr<V2_1::implementation::EventMessageQueueWrapperBase>& eventQueue,
                                const ::android::hardware::MQDescriptorSync<uint32_t>& wakeLockDescriptor,
                                const sp<ISensorsCallback>& sensorsCallback) {
    Result result = Result::OK;

    // Ensure that all sensors are disabled
    for (auto sensor : mSensors) {
      sensor.second->activate(false /* enable */);
    }

    // Stop the Wake Lock thread if it is currently running
    if (mReadWakeLockQueueRun.load()) {
      mReadWakeLockQueueRun = false;
      mWakeLockThread.join();
    }

    // Save a reference to the callback
    mCallback = sensorsCallback;

    // Reset direct channels
    for (auto& [channelHandle, channel] : mDirectChannels) {
      for (auto sensorHandle : channel->sensorHandles) {
        auto sensor = mSensors.find(sensorHandle);
        if (sensor != mSensors.end()) {
          sensor->second->removeDirectChannel(channelHandle);
        }
      }
    }
    mDirectChannels.clear();

    // Save the event queue.
    mEventQueue = std::move(eventQueue);

    // Ensure that any existing EventFlag is properly deleted
    deleteEventFlag();

    // Create the EventFlag that is used to signal to the framework that sensor
    // events have been written to the Event FMQ
    if (EventFlag::createEventFlag(mEventQueue->getEventFlagWord(), &mEventQueueFlag) != OK) {
      result = Result::BAD_VALUE;
    }

    // Create the Wake Lock FMQ that is used by the framework to communicate
    // whenever WAKE_UP events have been successfully read and handled by the
    // framework.
    mWakeLockQueue = std::make_unique<WakeLockMessageQueue>(wakeLockDescriptor, true /* resetPointers */);

    if (!mCallback || !mEventQueue || !mWakeLockQueue || mEventQueueFlag == nullptr) {
      result = Result::BAD_VALUE;
    }

    // Start the thread to read events from the Wake Lock FMQ
    mReadWakeLockQueueRun = true;
    mWakeLockThread = std::thread(startReadWakeLockThread, this);

    return result;
  }

  Return<Result> batch(int32_t sensorHandle, int64_t samplingPeriodNs, int64_t maxReportLatencyNs) override {
    int64_t usedLatency = maxReportLatencyNs;
    if (maxReportLatencyNs > 0) {
      mReportLatencyNs[sensorHandle] = maxReportLatencyNs;
      for (auto sensor : mSensors) {
        if (sensor.second->isEnabled()) {
          auto latency = mReportLatencyNs.find(sensorHandle);
          if (latency != mReportLatencyNs.end()) {
            usedLatency = std::min(usedLatency, latency->second);
          }
        }
      }
    }

    auto sensor = mSensors.find(sensorHandle);
    if (sensor != mSensors.end()) {
      sensor->second->batch(samplingPeriodNs, usedLatency);
      return Result::OK;
    }
    return Result::BAD_VALUE;
  }

  Return<Result> flush(int32_t sensorHandle) override {
    auto sensor = mSensors.find(sensorHandle);
    if (sensor != mSensors.end()) {
      return sensor->second->flush();
    }
    return Result::BAD_VALUE;
  }

  Return<Result> injectSensorData(const Event& event) override {
    auto sensor = mSensors.find(event.sensorHandle);
    if (sensor != mSensors.end()) {
      return sensor->second->injectEvent(V2_1::implementation::convertToNewEvent(event));
    }

    return Result::BAD_VALUE;
  }

  Return<void> registerDirectChannel(const SharedMemInfo& mem,
                                     V2_0::ISensors::registerDirectChannel_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lock(mChannelMutex);

    if (mem.type != V1_0::SharedMemType::ASHMEM) {
      _hidl_cb(Result::INVALID_OPERATION, -1);
      return Void();
    }

    sensors_direct_mem_t directMem;
    if (!V1_0::implementation::convertFromSharedMemInfo(mem, &directMem)) {
      _hidl_cb(Result::BAD_VALUE, -1);
      return Void();
    }

    auto ch = std::make_unique<AshmemDirectChannel>(&directMem);

    if (ch->isValid()) {
      int32_t channelHandle = mNextChannelHandle++;
      mDirectChannels.insert(std::make_pair(channelHandle, std::move(ch)));
      _hidl_cb(Result::OK, channelHandle);
      return Void();
    } else {
      switch (ch->getError()) {
        case BAD_VALUE:
          _hidl_cb(Result::BAD_VALUE, -1);
          break;
        case NO_MEMORY:
          _hidl_cb(Result::NO_MEMORY, -1);
          break;
        default:
          _hidl_cb(Result::INVALID_OPERATION, -1);
          break;
      }
      return Void();
    }
  }

  Return<Result> unregisterDirectChannel(int32_t channelHandle) override {
    std::lock_guard<std::mutex> lock(mChannelMutex);

    auto channelIt = mDirectChannels.find(channelHandle);
    if (channelIt != mDirectChannels.end()) {
      for (auto sensorHandle : channelIt->second->sensorHandles) {
        auto sensorIt = mSensors.find(sensorHandle);
        if (sensorIt != mSensors.end()) {
          sensorIt->second->removeDirectChannel(channelHandle);
        }
      }
    }

    mDirectChannels.erase(channelHandle);

    return Result::OK;
  }

  Return<void> configDirectReport(int32_t sensorHandle, int32_t channelHandle, RateLevel rate,
                                  V2_0::ISensors::configDirectReport_cb _hidl_cb) override {
    std::lock_guard<std::mutex> lock(mChannelMutex);

    auto channelIt = mDirectChannels.find(channelHandle);
    if (channelIt == mDirectChannels.end()) {
      _hidl_cb(Result::BAD_VALUE, -1);
      return Void();
    }

    if (sensorHandle == -1 && rate == RateLevel::STOP) {
      for (auto sensor : channelIt->second->sensorHandles) {
        auto sensorIt = mSensors.find(sensor);
        if (sensorIt != mSensors.end()) {
          channelIt->second->rateNs[sensor] = 0;
          sensorIt->second->stopDirectChannel(channelHandle);
        }
      }
      _hidl_cb(Result::OK, -1);
      return Void();
    }

    auto sensorIt = mSensors.find(sensorHandle);
    if (sensorIt == mSensors.end()) {
      _hidl_cb(Result::BAD_VALUE, -1);
      return Void();
    }

    if (!(sensorIt->second->getSensorInfo().flags & V1_0::SensorFlagBits::DIRECT_CHANNEL_ASHMEM)) {
      _hidl_cb(Result::BAD_VALUE, -1);
      return Void();
    }

    const int32_t maxRate = (sensorIt->second->getSensorInfo().flags & V1_0::SensorFlagBits::MASK_DIRECT_REPORT) >>
                            static_cast<uint8_t>(V1_0::SensorFlagShift::DIRECT_REPORT);

    switch (rate) {
      case RateLevel::STOP:
        channelIt->second->rateNs[sensorHandle] = 0;
        break;
      case RateLevel::NORMAL:
        channelIt->second->rateNs[sensorHandle] = 20000000;
        break;
      case RateLevel::FAST:
        if (maxRate < static_cast<int32_t>(RateLevel::FAST)) {
          _hidl_cb(Result::BAD_VALUE, -1);
          return Void();
        }
        channelIt->second->rateNs[sensorHandle] = 5000000;
        break;
      case RateLevel::VERY_FAST:
        if (maxRate < static_cast<int32_t>(RateLevel::VERY_FAST)) {
          _hidl_cb(Result::BAD_VALUE, -1);
          return Void();
        }
        channelIt->second->rateNs[sensorHandle] = 1250000;
        break;
      default:
        _hidl_cb(Result::BAD_VALUE, -1);
        return Void();
    }

    channelIt->second->sensorHandles.push_back(sensorHandle);
    sensorIt->second->addDirectChannel(channelHandle, channelIt->second->rateNs[sensorHandle]);

    _hidl_cb(Result::OK, sensorHandle);

    return Void();
  }

  void postEvents(const std::vector<V2_1::Event>& events, bool wakeup) override {
    std::lock_guard<std::mutex> lock(mWriteLock);
    if (mEventQueue->write(events)) {
      mEventQueueFlag->wake(static_cast<uint32_t>(EventQueueFlagBits::READ_AND_PROCESS));

      if (wakeup) {
        // Keep track of the number of outstanding WAKE_UP events in order to
        // properly hold a wake lock until the framework has secured a wake lock
        updateWakeLock(events.size(), 0 /* eventsHandled */);
      }
    }
  }

  void writeToDirectBuffer(const std::vector<V2_1::Event>& events, int64_t samplingPeriodNs) override {
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
          sensors_event_t ev;
          V2_1::implementation::convertToSensorEvent(event, &ev);
          channel->write(&ev);
          channel->sampleCount[event.sensorHandle] = 0;
        }
      }
      mChannelMutex.unlock();
    }
  }

protected:
  /**
   * Add new sensors
   */
  void AddSensors() {
    for (const auto& sensor : mSensorList.getAvailableSensors()) {
      const auto& data = sensor->getSensorData();
      SensorInfo sensorInfo{};
      sensorInfo.sensorHandle = mNextHandle++;
      sensorInfo.name = data.sensorName;
      sensorInfo.vendor = data.vendor;
      sensorInfo.type = static_cast<SensorType>(data.type);
      sensorInfo.typeAsString = "";
      sensorInfo.version = 1;
      sensorInfo.fifoReservedEventCount = 0;
      sensorInfo.fifoMaxEventCount = 0;
      sensorInfo.requiredPermission = "";

      switch (data.reportMode) {
        case bosch::sensors::SensorReportingMode::ON_CHANGE:
          sensorInfo.flags |= V1_0::SensorFlagBits::ON_CHANGE_MODE;
          break;
        case bosch::sensors::SensorReportingMode::CONTINUOUS:
          sensorInfo.flags |= V1_0::SensorFlagBits::CONTINUOUS_MODE;
          sensorInfo.flags |= V1_0::SensorFlagBits::ADDITIONAL_INFO;
          sensorInfo.flags |= V1_0::SensorFlagBits::DIRECT_CHANNEL_ASHMEM;
          sensorInfo.flags |=
            (static_cast<int32_t>(RateLevel::FAST) << static_cast<uint8_t>(V1_0::SensorFlagShift::DIRECT_REPORT));
          break;
        case bosch::sensors::SensorReportingMode::ONE_SHOT:
          sensorInfo.flags |= V1_0::SensorFlagBits::ONE_SHOT_MODE;
          break;
        case bosch::sensors::SensorReportingMode::SPECIAL_REPORTING:
          sensorInfo.flags |= V1_0::SensorFlagBits::SPECIAL_REPORTING_MODE;
          break;
        default:
          ALOGW("Unknown report mode: %d", data.reportMode);
          break;
      }
      sensorInfo.minDelay = data.minDelayUs;
      sensorInfo.maxDelay = data.maxDelayUs;
      sensorInfo.power = data.power;
      sensorInfo.maxRange = data.range;
      sensorInfo.resolution = data.resolution;

      const auto sensors_config_list = readSensorsConfigFromXml();
      std::optional<std::vector<Configuration>> sensorconfig = std::nullopt;
      sensorconfig = getSensorConfiguration(*sensors_config_list, sensorInfo.name, sensorInfo.type);
      std::shared_ptr<Sensor> halSensor =
        std::make_shared<Sensor>(this /* callback */, sensorInfo, sensor, sensorconfig);
      mSensors[halSensor->getSensorInfo().sensorHandle] = halSensor;
      ALOGD("AddSensor[%d] %s", halSensor->getSensorInfo().sensorHandle, halSensor->getSensorInfo().name.c_str());
    }
  }

  /**
   * Utility function to delete the Event Flag
   */
  void deleteEventFlag() {
    status_t status = EventFlag::deleteEventFlag(&mEventQueueFlag);
    if (status != OK) {
      ALOGI("Failed to delete event flag: %d", status);
    }
  }

  static void startReadWakeLockThread(Sensors* sensors) { sensors->readWakeLockFMQ(); }

  /**
   * Function to read the Wake Lock FMQ and release the wake lock when
   * appropriate
   */
  void readWakeLockFMQ() {
    while (mReadWakeLockQueueRun.load()) {
      constexpr int64_t kReadTimeoutNs = 500 * 1000 * 1000;  // 500 ms
      uint32_t eventsHandled = 0;

      // Read events from the Wake Lock FMQ. Timeout after a reasonable amount
      // of time to ensure that any held wake lock is able to be released if it
      // is held for too long.
      mWakeLockQueue->readBlocking(&eventsHandled, 1 /* count */, 0 /* readNotification */,
                                   static_cast<uint32_t>(WakeLockQueueFlagBits::DATA_WRITTEN), kReadTimeoutNs);
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
      mAutoReleaseWakeLockTime =
        ::android::uptimeMillis() + static_cast<uint32_t>(SensorTimeout::WAKE_LOCK_SECONDS) * 1000;
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
          SensorTimeout::WAKE_LOCK_SECONDS);
        mOutstandingWakeUpEvents = 0;
      }

      if (mOutstandingWakeUpEvents == 0 && release_wake_lock(kWakeLockName) == 0) {
        mHasWakeLock = false;
      }
    }
  }

  /**
   * The Event FMQ where sensor events are written
   */
  std::unique_ptr<V2_1::implementation::EventMessageQueueWrapperBase> mEventQueue;

  /**
   * The Wake Lock FMQ that is read to determine when the framework has handled
   * WAKE_UP events
   */
  std::unique_ptr<WakeLockMessageQueue> mWakeLockQueue;

  /**
   * Event Flag to signal to the framework when sensor events are available to
   * be read
   */
  EventFlag* mEventQueueFlag;

  /**
   * Callback for asynchronous events, such as dynamic sensor connections.
   */
  sp<ISensorsCallback> mCallback;

  /**
   * A map of the report latency in nanoseconds
   */
  std::map<int32_t, int64_t> mReportLatencyNs;

  /**
   * A map of the available sensors
   */
  std::map<int32_t, std::shared_ptr<Sensor>> mSensors;

  /**
   * The next available sensor handle
   */
  int32_t mNextHandle;

  /**
   * A list of the available sensors
   */
  bosch::sensors::SensorList mSensorList;

  /**
   * Lock to protect writes to the FMQs
   */
  std::mutex mWriteLock;

  /**
   * Lock to protect acquiring and releasing the wake lock
   */
  std::mutex mWakeLockLock;

  /**
   * Track the number of WAKE_UP events that have not been handled by the
   * framework
   */
  uint32_t mOutstandingWakeUpEvents;

  /**
   * A thread to read the Wake Lock FMQ
   */
  std::thread mWakeLockThread;

  /**
   * Flag to indicate that the Wake Lock Thread should continue to run
   */
  std::atomic_bool mReadWakeLockQueueRun;

  /**
   * Track the time when the wake lock should automatically be released
   */
  int64_t mAutoReleaseWakeLockTime;

  /**
   * Flag to indicate if a wake lock has been acquired
   */
  bool mHasWakeLock;

  /**
   * Direct channel support
   */
  std::map<int32_t, std::unique_ptr<DirectChannelBase>> mDirectChannels;
  std::mutex mChannelMutex;
  int32_t mNextChannelHandle = 1;
};

}  // namespace implementation
}  // namespace V2_X
}  // namespace sensors
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_SENSORS_V2_X_SENSORS_H
