/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "Sensor.h"

#include <log/log.h>
#include <utils/SystemClock.h>

#include <cmath>
#include <iostream>

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace subhal {
namespace implementation {

using ::android::hardware::sensors::V1_0::EventPayload;
using ::android::hardware::sensors::V1_0::MetaDataEventType;
using ::android::hardware::sensors::V1_0::OperationMode;
using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V1_0::SensorFlagBits;
using ::android::hardware::sensors::V1_0::SensorStatus;
using ::android::hardware::sensors::V2_1::Event;
using ::android::hardware::sensors::V2_1::SensorInfo;
using ::android::hardware::sensors::V2_1::SensorType;

Result Sensor::getSensorPlacement(std::vector<AdditionalInfo>& additionalInfoFrames) {
  AdditionalInfo sensorPlacement;
  sensorPlacement.type = AdditionalInfoType::AINFO_SENSOR_PLACEMENT;
  sensorPlacement.serial = 0;
  memset(&sensorPlacement.u.data_float, 0, sizeof(sensorPlacement.u.data_float));
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
    memcpy(&sensorPlacement.u.data_float[i * N], &rotationMatrix[i * M], M * sizeof(float));
    sensorPlacement.u.data_float[i * N + M] = locationVector[i];
  }

  additionalInfoFrames.push_back(sensorPlacement);
  return Result::OK;
}

Result Sensor::getSensorTemperature(std::vector<AdditionalInfo>& additionalInfoFrames) {
  AdditionalInfo sensorTemperature;
  sensorTemperature.type = AdditionalInfoType::AINFO_INTERNAL_TEMPERATURE;
  sensorTemperature.serial = 0;
  memset(&sensorTemperature.u.data_float, 0, sizeof(sensorTemperature.u.data_float));

  const bool ret = mSensor->readSensorTemperature(&sensorTemperature.u.data_float[0]);
  if (!ret) return Result::BAD_VALUE;

  additionalInfoFrames.push_back(sensorTemperature);
  return Result::OK;
}

Sensor::Sensor(ISensorsEventCallback* callback, const SensorInfo& sensorInfo,
               std::shared_ptr<bosch::sensors::ISensorHal> sensor)
  : mSensorInfo(sensorInfo), mCallback(callback), mSensor(sensor) {
  mSamplingPeriodNs = sensorInfo.minDelay * 1000LL;
  mDirectChannelRateNs = std::numeric_limits<int64_t>::max();
}

Sensor::~Sensor() {
  mIsEnabled = false;
  mDirectChannelEnabled = false;
}

const SensorInfo& Sensor::getSensorInfo() const { return mSensorInfo; }

void Sensor::batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
  ALOGD("Sensor batch %s %ld %ld", mSensorInfo.name.c_str(), samplingPeriodNs, maxReportLatencyNs);

  int64_t usedSamplingPeriodNs;
  int64_t usedDirectChannelRateNs;
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mSamplingPeriodNs = std::clamp(samplingPeriodNs, static_cast<int64_t>(mSensorInfo.minDelay) * 1000,
                                   static_cast<int64_t>(mSensorInfo.maxDelay) * 1000);

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
      .timestamp = android::elapsedRealtimeNano(),
      .sensorHandle = mSensorInfo.sensorHandle,
      .sensorType = SensorType::ADDITIONAL_INFO,
      .u.additional = frame,
    });
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

Result Sensor::flush() {
  ALOGD("Sensor flush %s", mSensorInfo.name.c_str());

  // Only generate a flush complete event if the sensor is enabled and if the
  // sensor is not a one-shot sensor.
  if (!mIsEnabled || (mSensorInfo.flags & static_cast<uint32_t>(SensorFlagBits::ONE_SHOT_MODE))) {
    return Result::BAD_VALUE;
  }

  {
    std::lock_guard<std::mutex> lock(mMutex);
    mFlushRequestCount++;
  }
  mSensor->flush();

  return Result::OK;
}

void Sensor::addDirectChannel(int32_t channelHandle, int64_t samplingPeriodNs) {
  ALOGD("Sensor addDirectChannel %s %d %ld", mSensorInfo.name.c_str(), channelHandle, samplingPeriodNs);

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
          event.u.meta.what = MetaDataEventType::META_DATA_FLUSH_COMPLETE;
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
            event.u.uncal.x = value.data[0];
            event.u.uncal.y = value.data[1];
            event.u.uncal.z = value.data[2];
            event.u.uncal.x_bias = 0;
            event.u.uncal.y_bias = 0;
            event.u.uncal.z_bias = 0;
          } else {
            ALOGE("Read failed: %zu", static_cast<size_t>(value.data.size()));
          }
        } else {
          if (value.data.size() == xyzLength) {
            event.u.vec3.x = value.data[0];
            event.u.vec3.y = value.data[1];
            event.u.vec3.z = value.data[2];
            event.u.vec3.status = SensorStatus::ACCURACY_HIGH;
            // ALOGD("%d: %f, %f, %f, %ld", event.sensorType, event.u.vec3.x, event.u.vec3.y, event.u.vec3.z,
            // event.timestamp);
          } else {
            ALOGE("Read failed: %lu", value.data.size());
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

bool Sensor::isWakeUpSensor() { return mSensorInfo.flags & static_cast<uint32_t>(SensorFlagBits::WAKE_UP); }

bool Sensor::supportsDataInjection() const {
  return mSensorInfo.flags & static_cast<uint32_t>(SensorFlagBits::DATA_INJECTION);
}

Result Sensor::injectEvent(const Event& event) {
  (void)(event);
  return Result::INVALID_OPERATION;
}

}  // namespace implementation
}  // namespace subhal
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
