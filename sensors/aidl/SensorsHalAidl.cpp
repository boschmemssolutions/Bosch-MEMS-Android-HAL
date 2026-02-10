/*
 * Copyright (C) 2021 The Android Open Source Project
 * Copyright (C) 2025 Robert Bosch GmbH
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

#include "sensors-impl/SensorsHalAidl.h"

#include <aidl/android/hardware/common/fmq/SynchronizedReadWrite.h>
#include <aidlcommonsupport/NativeHandle.h>

using ::aidl::android::hardware::common::fmq::MQDescriptor;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::aidl::android::hardware::sensors::Event;
using ::aidl::android::hardware::sensors::ISensors;
using ::aidl::android::hardware::sensors::ISensorsCallback;
using ::aidl::android::hardware::sensors::SensorInfo;
using ::ndk::ScopedAStatus;

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

ScopedAStatus SensorsHalAidl::activate(int32_t in_sensorHandle, bool in_enabled) {
  auto sensor = mSensors.find(in_sensorHandle);
  if (sensor != mSensors.end()) {
    sensor->second->activate(in_enabled);
    return ScopedAStatus::ok();
  }

  return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ScopedAStatus SensorsHalAidl::batch(int32_t in_sensorHandle, int64_t in_samplingPeriodNs,
                                    int64_t in_maxReportLatencyNs) {
  auto sensor = mSensors.find(in_sensorHandle);
  if (sensor != mSensors.end()) {
    sensor->second->batch(in_samplingPeriodNs, in_maxReportLatencyNs);
    return ScopedAStatus::ok();
  }

  return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

void SensorsHalAidl::AddSensors() {
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
      case bosch::sensors::SensorReportingMode::CONTINUOUS:
        sensorInfo.flags |= SensorInfo::SENSOR_FLAG_BITS_CONTINUOUS_MODE;
        sensorInfo.flags |= SensorInfo::SENSOR_FLAG_BITS_ADDITIONAL_INFO;
        sensorInfo.flags |= SensorInfo::SENSOR_FLAG_BITS_DIRECT_CHANNEL_ASHMEM;
        sensorInfo.flags |=
          (static_cast<int32_t>(ISensors::RateLevel::NORMAL) << SensorInfo::SENSOR_FLAG_SHIFT_DIRECT_REPORT);
        break;
      case bosch::sensors::SensorReportingMode::ON_CHANGE:
        sensorInfo.flags |= SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE;
        break;
      case bosch::sensors::SensorReportingMode::ONE_SHOT:
        sensorInfo.flags |= SensorInfo::SENSOR_FLAG_BITS_ONE_SHOT_MODE;
        break;
      case bosch::sensors::SensorReportingMode::SPECIAL_REPORTING:
        sensorInfo.flags |= SensorInfo::SENSOR_FLAG_BITS_SPECIAL_REPORTING_MODE;
        break;
      default:
        ALOGW("Unknown sensor reporting mode: %d", data.reportMode);
        break;
    }
    sensorInfo.minDelayUs = data.minDelayUs;
    sensorInfo.maxDelayUs = data.maxDelayUs;
    sensorInfo.power = data.power;
    sensorInfo.maxRange = data.range;
    sensorInfo.resolution = data.resolution;

    std::shared_ptr<Sensor> halSensor = std::make_shared<Sensor>(this /* callback */, sensorInfo, sensor);
    mSensors[halSensor->getSensorInfo().sensorHandle] = halSensor;
    ALOGD("AddSensor[%d] %s", halSensor->getSensorInfo().sensorHandle, halSensor->getSensorInfo().name.c_str());
  }
}

ScopedAStatus SensorsHalAidl::configDirectReport(int32_t in_sensorHandle, int32_t in_channelHandle,
                                                 ISensors::RateLevel in_rate, int32_t* _aidl_return) {
  std::lock_guard<std::mutex> lock(mChannelMutex);

  auto channelIt = mDirectChannels.find(in_channelHandle);
  if (channelIt == mDirectChannels.end()) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  if (in_sensorHandle == -1 && in_rate == ISensors::RateLevel::STOP) {
    for (auto sensor : channelIt->second->sensorHandles) {
      auto sensorIt = mSensors.find(sensor);
      if (sensorIt != mSensors.end()) {
        channelIt->second->rateNs[sensor] = 0;
        sensorIt->second->stopDirectChannel(in_channelHandle);
      }
    }
    return ndk::ScopedAStatus::ok();
  }

  auto sensorIt = mSensors.find(in_sensorHandle);
  if (sensorIt == mSensors.end()) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  if (!(sensorIt->second->getSensorInfo().flags & SensorInfo::SENSOR_FLAG_BITS_DIRECT_CHANNEL_ASHMEM)) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  const int32_t maxRate = (sensorIt->second->getSensorInfo().flags & SENSOR_FLAG_MASK_DIRECT_REPORT) >>
                          SensorInfo::SENSOR_FLAG_SHIFT_DIRECT_REPORT;

  switch (in_rate) {
    case ISensors::RateLevel::STOP:
      channelIt->second->rateNs[in_sensorHandle] = 0;
      break;
    case ISensors::RateLevel::NORMAL:
      channelIt->second->rateNs[in_sensorHandle] = 20000000;
      break;
    case ISensors::RateLevel::FAST:
      if (maxRate < static_cast<int32_t>(ISensors::RateLevel::FAST)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
      }
      channelIt->second->rateNs[in_sensorHandle] = 5000000;
      break;
    case ISensors::RateLevel::VERY_FAST:
      if (maxRate < static_cast<int32_t>(ISensors::RateLevel::VERY_FAST)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
      }
      channelIt->second->rateNs[in_sensorHandle] = 1250000;
      break;
    default:
      return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  channelIt->second->sensorHandles.insert(in_sensorHandle);
  sensorIt->second->addDirectChannel(in_channelHandle, channelIt->second->rateNs[in_sensorHandle]);

  *_aidl_return = in_sensorHandle;
  return ndk::ScopedAStatus::ok();
}

ScopedAStatus SensorsHalAidl::flush(int32_t in_sensorHandle) {
  auto sensor = mSensors.find(in_sensorHandle);
  if (sensor != mSensors.end()) {
    return sensor->second->flush();
  }

  return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ScopedAStatus SensorsHalAidl::getSensorsList(std::vector<SensorInfo>* _aidl_return) {
  for (const auto& sensor : mSensors) {
    _aidl_return->push_back(sensor.second->getSensorInfo());
  }
  return ScopedAStatus::ok();
}

ScopedAStatus SensorsHalAidl::initialize(
  const MQDescriptor<Event, SynchronizedReadWrite>& in_eventQueueDescriptor,
  const MQDescriptor<int32_t, SynchronizedReadWrite>& in_wakeLockDescriptor,
  const std::shared_ptr<::aidl::android::hardware::sensors::ISensorsCallback>& in_sensorsCallback) {
  ScopedAStatus result = ScopedAStatus::ok();

  // Ensure that all sensors are disabled.
  for (auto sensor : mSensors) {
    sensor.second->activate(false);
  }

  // Stop the Wake Lock thread if it is currently running
  if (mReadWakeLockQueueRun.load()) {
    mReadWakeLockQueueRun = false;
    mWakeLockThread.join();
  }

  mEventQueue =
    std::make_unique<AidlMessageQueue<Event, SynchronizedReadWrite>>(in_eventQueueDescriptor, true /* resetPointers */);

  // Save a reference to the callback
  mCallback = in_sensorsCallback;

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

  // Ensure that any existing EventFlag is properly deleted
  deleteEventFlag();

  // Create the EventFlag that is used to signal to the framework that sensor events have been
  // written to the Event FMQ
  if (EventFlag::createEventFlag(mEventQueue->getEventFlagWord(), &mEventQueueFlag) != OK) {
    result = ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  // Create the Wake Lock FMQ that is used by the framework to communicate whenever WAKE_UP
  // events have been successfully read and handled by the framework.
  mWakeLockQueue =
    std::make_unique<AidlMessageQueue<int32_t, SynchronizedReadWrite>>(in_wakeLockDescriptor, true /* resetPointers */);

  if (!mCallback || !mEventQueue || !mWakeLockQueue || mEventQueueFlag == nullptr) {
    result = ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  // Start the thread to read events from the Wake Lock FMQ
  mReadWakeLockQueueRun = true;
  mWakeLockThread = std::thread(startReadWakeLockThread, this);
  return result;
}

ScopedAStatus SensorsHalAidl::injectSensorData(const Event& in_event) {
  auto sensor = mSensors.find(in_event.sensorHandle);
  if (sensor != mSensors.end()) {
    return sensor->second->injectEvent(in_event);
  }
  return ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(ERROR_BAD_VALUE));
}

ScopedAStatus SensorsHalAidl::registerDirectChannel(const ISensors::SharedMemInfo& in_mem, int32_t* _aidl_return) {
  std::lock_guard<std::mutex> lock(mChannelMutex);

  if (in_mem.type != ISensors::SharedMemInfo::SharedMemType::ASHMEM) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  sensors_direct_mem_t directMem = {.type = static_cast<int>(in_mem.type),
                                    .format = static_cast<int>(in_mem.format),
                                    .size = static_cast<size_t>(in_mem.size),
                                    .handle = ::android::makeFromAidl(in_mem.memoryHandle)};

  auto ch = std::make_unique<::android::AshmemDirectChannel>(&directMem);

  if (ch->isValid()) {
    int32_t channelHandle = mNextChannelHandle++;
    mDirectChannels.insert(std::make_pair(channelHandle, std::move(ch)));
    *_aidl_return = channelHandle;
    return ndk::ScopedAStatus::ok();
  } else {
    return ndk::ScopedAStatus::fromServiceSpecificError(ch->getError());
  }
}  // namespace sensors

ScopedAStatus SensorsHalAidl::setOperationMode(OperationMode in_mode) {
  auto res = ScopedAStatus::ok();
  for (auto sensor : mSensors) {
    res = sensor.second->setOperationMode(in_mode);
  }
  return res;
}

ScopedAStatus SensorsHalAidl::unregisterDirectChannel(int32_t in_channelHandle) {
  std::lock_guard<std::mutex> lock(mChannelMutex);

  auto channelIt = mDirectChannels.find(in_channelHandle);
  if (channelIt != mDirectChannels.end()) {
    for (auto sensorHandle : channelIt->second->sensorHandles) {
      auto sensorIt = mSensors.find(sensorHandle);
      if (sensorIt != mSensors.end()) {
        sensorIt->second->removeDirectChannel(in_channelHandle);
      }
    }
    mDirectChannels.erase(in_channelHandle);
  }

  return ndk::ScopedAStatus::ok();
}

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
