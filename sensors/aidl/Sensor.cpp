/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "sensors-impl/Sensor.h"

#include <log/log.h>
#include <utils/SystemClock.h>

#include <cmath>
#include <iostream>

using ::ndk::ScopedAStatus;

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

ndk::ScopedAStatus Sensor::getSensorPlacement(std::vector<AdditionalInfo>& additionalInfoFrames) {
  AdditionalInfo sensorPlacement;
  sensorPlacement.type = AdditionalInfoType::AINFO_SENSOR_PLACEMENT;
  sensorPlacement.serial = 0;

  AdditionalInfo::AdditionalInfoPayload::FloatValues sensorPlacementPayload;
  memset(&sensorPlacementPayload.values, 0, sizeof(sensorPlacementPayload.values));
  // SensorPlacementData is given as a 3x4 matrix consisting of a 3x3 rotation matrix
  // concatenated with a 3x1 location vector in row major order. Example: This raw buffer:
  // {x1,y1,z1,l1,x2,y2,z2,l2,x3,y3,z3,l3} corresponds to the following 3x4 matrix:
  //  x1 y1 z1 l1
  //  x2 y2 z2 l2
  //  x3 y3 z3 l3
  constexpr size_t M = 3;
  constexpr size_t N = 4;

  const auto& rotationMatrix = mSensor->getRotationMatrix();
  const auto& locationVector = mSensor->getLocationVector();

  for (size_t i = 0; i < M; ++i) {
    memcpy(&sensorPlacementPayload.values[i * N], &rotationMatrix[i * M], M * sizeof(float));
    sensorPlacementPayload.values[i * N + M] = locationVector[i];
  }

  sensorPlacement.payload.set<AdditionalInfo::AdditionalInfoPayload::dataFloat>(sensorPlacementPayload);
  additionalInfoFrames.push_back(sensorPlacement);
  return ScopedAStatus::ok();
}

ndk::ScopedAStatus Sensor::getSensorTemperature(std::vector<AdditionalInfo>& additionalInfoFrames) {
  AdditionalInfo sensorTemperature;
  AdditionalInfo::AdditionalInfoPayload::FloatValues additionalInfoValues;
  sensorTemperature.type = AdditionalInfoType::AINFO_INTERNAL_TEMPERATURE;
  sensorTemperature.serial = 0;
  memset(&additionalInfoValues.values, 0, sizeof(additionalInfoValues.values));

  const bool ret = mSensor->readSensorTemperature(&additionalInfoValues.values[0]);
  if (!ret) return ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);

  sensorTemperature.payload.set<AdditionalInfo::AdditionalInfoPayload::dataFloat>(additionalInfoValues);
  additionalInfoFrames.push_back(sensorTemperature);
  return ScopedAStatus::ok();
}

Sensor::Sensor(ISensorsEventCallback* callback, const SensorInfo& sensorInfo,
               std::shared_ptr<bosch::sensors::ISensorHal> sensor)
  : mSensorInfo(sensorInfo), mCallback(callback), mSensor(sensor) {
  mSamplingPeriodNs = sensorInfo.minDelayUs * 1000LL;
  mDirectChannelRateNs = std::numeric_limits<int64_t>::max();
}

Sensor::~Sensor() {
  mIsEnabled = false;
  mDirectChannelEnabled = false;
}

const SensorInfo& Sensor::getSensorInfo() const { return mSensorInfo; }

void Sensor::batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
  ALOGD("Sensor batch %s %zu %zu", mSensorInfo.name.c_str(), static_cast<size_t>(samplingPeriodNs),
        static_cast<size_t>(maxReportLatencyNs));

  int64_t usedSamplingPeriodNs;
  int64_t usedDirectChannelRateNs;
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mSamplingPeriodNs = samplingPeriodNs;

    if (mSamplingPeriodNs < mSensorInfo.minDelayUs * 1000LL) {
      mSamplingPeriodNs = mSensorInfo.minDelayUs * 1000LL;
    } else if (mSamplingPeriodNs > mSensorInfo.maxDelayUs * 1000LL) {
      mSamplingPeriodNs = mSensorInfo.maxDelayUs * 1000LL;
    }

    usedSamplingPeriodNs = mSamplingPeriodNs;
    usedDirectChannelRateNs = mDirectChannelRateNs;
  }

  if (usedSamplingPeriodNs < usedDirectChannelRateNs) {
    mSensor->batch(usedSamplingPeriodNs, maxReportLatencyNs);
  }
}

void Sensor::sendAdditionalInfoReport() {
  std::vector<Event> events;
  std::vector<AdditionalInfo> additionalInfoFrames;

  additionalInfoFrames.push_back({
    .type = AdditionalInfoType::AINFO_BEGIN,
    .serial = 0,
  });

  getSensorPlacement(additionalInfoFrames);
  getSensorTemperature(additionalInfoFrames);

  additionalInfoFrames.push_back({
    .type = AdditionalInfoType::AINFO_END,
    .serial = 0,
  });

  for (const auto& frame : additionalInfoFrames) {
    events.emplace_back(Event{
      .sensorHandle = mSensorInfo.sensorHandle,
      .sensorType = SensorType::ADDITIONAL_INFO,
      .timestamp = ::android::elapsedRealtimeNano(),
    });
    events.back().payload.set<EventPayload::Tag::additional>(frame);
  }

  if (!events.empty()) {
    mCallback->postEvents(events, isWakeUpSensor());
  }
}

void Sensor::activate(bool enable) {
  ALOGD("Sensor activate %s %d", mSensorInfo.name.c_str(), enable);

  bool updateNeeded = false;
  {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mIsEnabled != enable) {
      updateNeeded = true;
      mIsEnabled = enable;
      mNextSampleTimeNs = mIsEnabled ? 0 : std::numeric_limits<int64_t>::max();
    }
  }

  if (updateNeeded) {
    mSensor->activate(enable);
    if (enable) {
      mSensor->registerObserver(shared_from_this());
      sendAdditionalInfoReport();
    } else {
      mSensor->removeObserver(shared_from_this());
    }
  }
}

bool Sensor::isEnabled() { return mIsEnabled; }

ScopedAStatus Sensor::flush() {
  ALOGD("Sensor flush %s", mSensorInfo.name.c_str());

  // Only generate a flush complete event if the sensor is enabled and if the
  // sensor is not a one-shot sensor.
  if (!mIsEnabled || (mSensorInfo.flags & static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ONE_SHOT_MODE))) {
    return ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(BnSensors::ERROR_BAD_VALUE));
  }

  {
    std::lock_guard<std::mutex> lock(mMutex);
    mFlushRequestCount++;
  }
  mSensor->flush();

  return ScopedAStatus::ok();
}

void Sensor::addDirectChannel(int32_t channelHandle, int64_t samplingPeriodNs) {
  ALOGD("Sensor addDirectChannel %s %d %zu", mSensorInfo.name.c_str(), channelHandle,
        static_cast<size_t>(samplingPeriodNs));

  if (samplingPeriodNs == 0) {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mDirectChannels[channelHandle] = {false, 0};
    }
    stopDirectChannel(channelHandle);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mMutex);
    mDirectChannels[channelHandle] = {true, samplingPeriodNs};
  }
  updateDirectChannel();
}

void Sensor::stopDirectChannel(int32_t channelHandle) {
  ALOGD("Sensor stopDirectChannel %s %d", mSensorInfo.name.c_str(), channelHandle);
  {
    std::lock_guard<std::mutex> lock(mMutex);
    auto itRateNs = mDirectChannels.find(channelHandle);
    if (itRateNs != mDirectChannels.end()) {
      itRateNs->second.enabled = false;
      itRateNs->second.samplingPeriodNs = 0;
    }
  }
  updateDirectChannel();
}

void Sensor::removeDirectChannel(int32_t channelHandle) {
  ALOGD("Sensor removeDirectChannel %s %d", mSensorInfo.name.c_str(), channelHandle);
  {
    std::lock_guard<std::mutex> lock(mMutex);
    auto itEnabled = mDirectChannels.find(channelHandle);
    if (itEnabled != mDirectChannels.end()) {
      mDirectChannels.erase(itEnabled);
    }
  }
  updateDirectChannel();
}

void Sensor::updateDirectChannel() {
  bool anyChannelEnabled = false;
  bool needsBatch = false;
  bool needsActivate = true;
  int64_t directChannelRateNs = std::numeric_limits<int64_t>::max();
  {
    std::lock_guard<std::mutex> lock(mMutex);

    for (const auto& [_, channel] : mDirectChannels) {
      if (channel.enabled) {
        anyChannelEnabled = true;
        if (channel.samplingPeriodNs < directChannelRateNs) {
          directChannelRateNs = channel.samplingPeriodNs;
        }
      }
    }

    if (mDirectChannelRateNs != directChannelRateNs) {
      mDirectChannelRateNs = directChannelRateNs;
      if (anyChannelEnabled && (!mIsEnabled || (mDirectChannelRateNs < mSamplingPeriodNs))) {
        needsBatch = true;
      }
    }

    if (mDirectChannelEnabled != anyChannelEnabled) {
      mDirectChannelEnabled = anyChannelEnabled;
      mNextDirectChannelNs = mDirectChannelEnabled ? 0 : std::numeric_limits<int64_t>::max();
      if (!mIsEnabled) {
        needsActivate = true;
      }
    }
  }

  if (needsBatch) {
    mSensor->batch(directChannelRateNs, 0);
  }

  if (needsActivate) {
    mSensor->activate(anyChannelEnabled);

    if (anyChannelEnabled) {
      mSensor->registerObserver(shared_from_this());
    } else {
      mSensor->removeObserver(shared_from_this());
    }
  }
}

void Sensor::update(const std::vector<bosch::sensors::SensorValues>& values) {
  std::vector<Event> events;
  std::vector<Event> directEvents;
  const size_t xyzLength = 3;
  bool needsFlush = false;

  {
    std::lock_guard<std::mutex> lock(mMutex);

    for (const auto& value : values) {
      if (static_cast<int>(value.type) == static_cast<int>(SensorType::META_DATA)) {
        if ((mFlushRequestCount > 0) && value.flushRequest) {
          Event event{};
          event.sensorHandle = mSensorInfo.sensorHandle;
          event.sensorType = SensorType::META_DATA;
          EventPayload::MetaData meta = {
            .what = MetaDataEventType::META_DATA_FLUSH_COMPLETE,
          };
          event.payload.set<EventPayload::Tag::meta>(meta);
          events.push_back(event);
          mFlushRequestCount--;
          needsFlush = true;
        }
      } else if (static_cast<int>(value.type) == static_cast<int>(mSensorInfo.type)) {
        Event event{};
        event.sensorHandle = mSensorInfo.sensorHandle;
        event.sensorType = mSensorInfo.type;
        event.timestamp = value.timestamp;

        if ((mSensorInfo.type == SensorType::GYROSCOPE_UNCALIBRATED) ||
            (mSensorInfo.type == SensorType::ACCELEROMETER_UNCALIBRATED)) {
          if (value.data.size() == xyzLength) {
            EventPayload::Uncal uncal = {
              .x = value.data[0],
              .y = value.data[1],
              .z = value.data[2],
              .xBias = 0,
              .yBias = 0,
              .zBias = 0,
            };
            event.payload.set<EventPayload::Tag::uncal>(uncal);
            // ALOGD("%f, %f, %f, %zu", uncal.x, uncal.y, uncal.z, static_cast<size_t>(event.timestamp));
          } else {
            ALOGE("Read failed: %zu", static_cast<size_t>(value.data.size()));
          }
        } else {
          if (value.data.size() == xyzLength) {
            EventPayload::Vec3 vec3 = {
              .x = value.data[0],
              .y = value.data[1],
              .z = value.data[2],
              .status = SensorStatus::ACCURACY_HIGH,
            };
            event.payload.set<EventPayload::Tag::vec3>(vec3);
            // ALOGD("%f, %f, %f, %zu", vec3.x, vec3.y, vec3.z, static_cast<size_t>(event.timestamp));
          } else {
            ALOGE("Read failed: %zu", static_cast<size_t>(value.data.size()));
            continue;
          }
        }

        if (mDirectChannelEnabled) {
          if (event.timestamp >= mNextDirectChannelNs) {
            mNextDirectChannelNs = event.timestamp + mDirectChannelRateNs * bosch::sensors::MIN_VALID_TIMESTAMP_FACTOR;
            directEvents.push_back(event);
          }
        }
        if (mIsEnabled) {
          if (event.timestamp >= mNextSampleTimeNs) {
            mNextSampleTimeNs = event.timestamp + mSamplingPeriodNs * bosch::sensors::MIN_VALID_TIMESTAMP_FACTOR;
            events.push_back(event);
          }
        }
      }
    }
  }

  if (!directEvents.empty()) {
    mCallback->writeToDirectBuffer(directEvents);
  }
  if (!events.empty()) {
    mCallback->postEvents(events, isWakeUpSensor());
  }

  if (needsFlush) {
    sendAdditionalInfoReport();
  }
}

bool Sensor::isWakeUpSensor() {
  return mSensorInfo.flags & static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_WAKE_UP);
}

ScopedAStatus Sensor::setOperationMode(OperationMode mode) {
  if (mode == OperationMode::NORMAL) {
    return ScopedAStatus::ok();
  } else {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }
}

bool Sensor::supportsDataInjection() const {
  return mSensorInfo.flags & static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_DATA_INJECTION);
}

ScopedAStatus Sensor::injectEvent(const Event& event) {
  if (event.sensorType == SensorType::ADDITIONAL_INFO) {
    return ScopedAStatus::ok();
    // When in OperationMode::NORMAL, SensorType::ADDITIONAL_INFO is used to
    // push operation environment data into the device.
  }

  if (!supportsDataInjection()) {
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
  }

  return ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(BnSensors::ERROR_BAD_VALUE));
}

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
