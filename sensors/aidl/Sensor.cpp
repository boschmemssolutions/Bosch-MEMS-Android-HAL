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

#include "sensors-impl/Sensor.h"

#include <log/log.h>

using ::ndk::ScopedAStatus;

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

Sensor::Sensor(ISensorsEventCallback* callback, const SensorInfo& sensorInfo,
               std::shared_ptr<bosch::sensors::ISensorHal> sensor)
  : mIsEnabled(false),
    mLastSampleTimeNs(0),
    mSensorInfo(sensorInfo),
    mStopThread(false),
    mCallback(callback),
    mSensor(sensor) {
  mRunThread = std::thread(startThread, this);
  mSamplingPeriodNs = sensorInfo.minDelayUs * 1000LL;
}

Sensor::~Sensor() {
  std::unique_lock<std::mutex> lock(mRunMutex);
  mStopThread = true;
  mIsEnabled = false;
  mWaitCV.notify_all();
  lock.release();
  mRunThread.join();
}

const SensorInfo& Sensor::getSensorInfo() const { return mSensorInfo; }

void Sensor::batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
  if (samplingPeriodNs < mSensorInfo.minDelayUs * 1000LL) {
    samplingPeriodNs = mSensorInfo.minDelayUs * 1000LL;
  } else if (samplingPeriodNs > mSensorInfo.maxDelayUs * 1000LL) {
    samplingPeriodNs = mSensorInfo.maxDelayUs * 1000LL;
  }

  ALOGD("Sensor batch %s %zu %zu", mSensorInfo.name.c_str(), static_cast<size_t>(samplingPeriodNs),
        static_cast<size_t>(maxReportLatencyNs));
  mSensor->batch(samplingPeriodNs, maxReportLatencyNs);

  if (mSamplingPeriodNs != samplingPeriodNs) {
    mSamplingPeriodNs = samplingPeriodNs;
    // Wake up the 'run' thread to check if a new event should be generated now
    mWaitCV.notify_all();
  }
}

void Sensor::activate(bool enable) {
  ALOGD("Sensor activate %s %d", mSensorInfo.name.c_str(), enable);
  if (mIsEnabled != enable) {
    std::unique_lock<std::mutex> lock(mRunMutex);
    mIsEnabled = enable;
    mWaitCV.notify_all();
    mSensor->activate(enable);
  }
}

bool Sensor::isEnabled() { return mIsEnabled; }

ScopedAStatus Sensor::flush() {
  // Only generate a flush complete event if the sensor is enabled and if the
  // sensor is not a one-shot sensor.
  if (!mIsEnabled || (mSensorInfo.flags & static_cast<uint32_t>(SensorInfo::SENSOR_FLAG_BITS_ONE_SHOT_MODE))) {
    return ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(BnSensors::ERROR_BAD_VALUE));
  }

  // Note: If a sensor supports batching, write all of the currently batched
  // events for the sensor to the Event FMQ prior to writing the flush complete
  // event.
  Event ev;
  ev.sensorHandle = mSensorInfo.sensorHandle;
  ev.sensorType = SensorType::META_DATA;
  EventPayload::MetaData meta = {
    .what = MetaDataEventType::META_DATA_FLUSH_COMPLETE,
  };
  ev.payload.set<EventPayload::Tag::meta>(meta);
  std::vector<Event> evs{ev};
  mCallback->postEvents(evs, isWakeUpSensor());

  return ScopedAStatus::ok();
}

void Sensor::startThread(Sensor* sensor) { sensor->run(); }

void Sensor::run() {
  std::unique_lock<std::mutex> runLock(mRunMutex);
  constexpr int64_t kNanosecondsInSeconds = 1000 * 1000 * 1000;

  while (!mStopThread) {
    if (!mIsEnabled) {
      mWaitCV.wait(runLock, [&] { return (mIsEnabled || mStopThread); });
    } else {
      timespec curTime;
      clock_gettime(CLOCK_BOOTTIME, &curTime);
      int64_t now = (curTime.tv_sec * kNanosecondsInSeconds) + curTime.tv_nsec;
      int64_t nextSampleTime = mLastSampleTimeNs + mSamplingPeriodNs;

      if (now >= nextSampleTime) {
        mLastSampleTimeNs = now;
        nextSampleTime = mLastSampleTimeNs + mSamplingPeriodNs;
        mCallback->postEvents(readEvents(), isWakeUpSensor());
      }
      clock_gettime(CLOCK_BOOTTIME, &curTime);
      now = (curTime.tv_sec * kNanosecondsInSeconds) + curTime.tv_nsec;
      mWaitCV.wait_for(runLock, std::chrono::nanoseconds(nextSampleTime - now));
    }
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

std::vector<Event> Sensor::readEvents() {
  std::vector<Event> events;
  const auto& values = mSensor->readSensorValues();
  const size_t xyzLength = 3;

  for (const auto& value : values) {
    Event event{};
    event.sensorHandle = mSensorInfo.sensorHandle;
    event.sensorType = mSensorInfo.type;
    event.timestamp = value.timestamp;
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
    }
    events.push_back(event);
  }

  return events;
}

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
