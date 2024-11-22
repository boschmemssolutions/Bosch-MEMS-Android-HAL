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

static Result setSensorPlacementData(AdditionalInfo* sensorPlacement, int index, float value) {
  if (!sensorPlacement) return Result::BAD_VALUE;
  int arraySize = sizeof(sensorPlacement->u.data_float) / sizeof(sensorPlacement->u.data_float[0]);
  if (index < 0 || index >= arraySize) return Result::BAD_VALUE;
  sensorPlacement->u.data_float[index] = value;
  return Result::OK;
}

static std::optional<std::vector<Orientation>> getOrientation(std::optional<std::vector<Configuration>> config) {
  if (!config) return std::nullopt;
  if (config->empty()) return std::nullopt;
  Configuration& sensorCfg = (*config)[0];
  return sensorCfg.getOrientation();
}

static std::optional<std::vector<Location>> getLocation(std::optional<std::vector<Configuration>> config) {
  if (!config) return std::nullopt;
  if (config->empty()) return std::nullopt;
  Configuration& sensorCfg = (*config)[0];
  return sensorCfg.getLocation();
}

Result Sensor::getSensorPlacement(std::vector<AdditionalInfo>& additionalInfoFrames) {
  AdditionalInfo sensorPlacement;
  auto sensorLocationList = getLocation(mConfig);
  if (!sensorLocationList) return Result::BAD_VALUE;
  if (sensorLocationList->empty()) return Result::BAD_VALUE;
  auto sensorOrientationList = getOrientation(mConfig);
  if (!sensorOrientationList) return Result::BAD_VALUE;
  if (sensorOrientationList->empty()) return Result::BAD_VALUE;
  sensorPlacement.type = AdditionalInfoType::AINFO_SENSOR_PLACEMENT;
  sensorPlacement.serial = 0;
  memset(&sensorPlacement.u.data_float, 0, sizeof(sensorPlacement.u.data_float));
  Location& sensorLocation = (*sensorLocationList)[0];
  // SensorPlacementData is given as a 3x4 matrix consisting of a 3x3 rotation matrix (R)
  // concatenated with a 3x1 location vector (t) in row major order. Example: This raw buffer:
  // {x1,y1,z1,l1,x2,y2,z2,l2,x3,y3,z3,l3} corresponds to the following 3x4 matrix:
  //  x1 y1 z1 l1
  //  x2 y2 z2 l2
  //  x3 y3 z3 l3
  // LOCATION_X_IDX,LOCATION_Y_IDX,LOCATION_Z_IDX corresponds to the indexes of the location
  // vector (l1,l2,l3) in the raw buffer.
  Result ret = setSensorPlacementData(&sensorPlacement, Sensor::LOCATION_X_IDX, sensorLocation.getX());
  if (ret != Result::OK) return ret;
  ret = setSensorPlacementData(&sensorPlacement, Sensor::LOCATION_Y_IDX, sensorLocation.getY());
  if (ret != Result::OK) return ret;
  ret = setSensorPlacementData(&sensorPlacement, Sensor::LOCATION_Z_IDX, sensorLocation.getZ());
  if (ret != Result::OK) return ret;
  Orientation& sensorOrientation = (*sensorOrientationList)[0];
  if (sensorOrientation.getRotate()) {
    // If the HAL is already rotating the sensor orientation to align with the Android
    // Coordinate system, then the sensor rotation matrix will be an identity matrix
    // ROTATION_X_IDX, ROTATION_Y_IDX, ROTATION_Z_IDX corresponds to indexes of the
    // (x1,y1,z1) in the raw buffer.
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_X_IDX + 0, 1);
    if (ret != Result::OK) return ret;
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_Y_IDX + 4, 1);
    if (ret != Result::OK) return ret;
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_Z_IDX + 8, 1);
    if (ret != Result::OK) return ret;
  } else {
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_X_IDX + 4 * sensorOrientation.getFirstX()->getMap(),
                                 sensorOrientation.getFirstX()->getNegate() ? -1 : 1);
    if (ret != Result::OK) return ret;
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_Y_IDX + 4 * sensorOrientation.getFirstY()->getMap(),
                                 sensorOrientation.getFirstY()->getNegate() ? -1 : 1);
    if (ret != Result::OK) return ret;
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_Z_IDX + 4 * sensorOrientation.getFirstZ()->getMap(),
                                 sensorOrientation.getFirstZ()->getNegate() ? -1 : 1);
    if (ret != Result::OK) return ret;
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
               std::shared_ptr<bosch::sensors::ISensorHal> sensor,
               const std::optional<std::vector<Configuration>>& config)
  : mIsEnabled(false),
    mLastSampleTimeNs(0),
    mSensorInfo(sensorInfo),
    mStopThread(false),
    mCallback(callback),
    mSensor(sensor),
    mConfig(config) {
  mRunThread = std::thread(startThread, this);
  mSamplingPeriodNs = sensorInfo.minDelay * 1000LL;
}

Sensor::~Sensor() {
  // Ensure that lock is unlocked before calling mRunThread.join() or a
  // deadlock will occur.
  {
    std::unique_lock<std::mutex> lock(mRunMutex);
    mStopThread = true;
    mIsEnabled = false;
    mWaitCV.notify_all();
  }
  mRunThread.join();
}

const SensorInfo& Sensor::getSensorInfo() const { return mSensorInfo; }

void Sensor::batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
  if (!mDirectChannels.empty()) return;

  samplingPeriodNs = std::clamp(samplingPeriodNs, static_cast<int64_t>(mSensorInfo.minDelay) * 1000,
                                static_cast<int64_t>(mSensorInfo.maxDelay) * 1000);

  ALOGD("Sensor batch %s %ld %ld", mSensorInfo.name.c_str(), samplingPeriodNs, maxReportLatencyNs);
  mSensor->batch(samplingPeriodNs, maxReportLatencyNs);

  if (mSamplingPeriodNs != samplingPeriodNs) {
    mSamplingPeriodNs = samplingPeriodNs;
    // Wake up the 'run' thread to check if a new event should be generated now
    mWaitCV.notify_all();
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
      .timestamp = android::elapsedRealtimeNano(),
      .u.additional = frame,
    });
  }
  if (!events.empty()) {
    mCallback->postEvents(events, isWakeUpSensor());
  }
}

void Sensor::activate(bool enable) {
  if (!mDirectChannels.empty()) return;

  ALOGD("Sensor activate %s %d", mSensorInfo.name.c_str(), enable);
  if (mIsEnabled != enable) {
    std::unique_lock<std::mutex> lock(mRunMutex);
    mIsEnabled = enable;
    mWaitCV.notify_all();
    mSensor->activate(enable);
    if (enable) sendAdditionalInfoReport();
  }
}

bool Sensor::isEnabled() { return mIsEnabled; }

Result Sensor::flush() {
  // Only generate a flush complete event if the sensor is enabled and if the
  // sensor is not a one-shot sensor.
  if (!mIsEnabled || (mSensorInfo.flags & static_cast<uint32_t>(SensorFlagBits::ONE_SHOT_MODE))) {
    return Result::BAD_VALUE;
  }

  // Note: If a sensor supports batching, write all of the currently batched
  // events for the sensor to the Event FMQ prior to writing the flush complete
  // event.
  Event ev;
  ev.sensorHandle = mSensorInfo.sensorHandle;
  ev.sensorType = SensorType::META_DATA;
  ev.u.meta.what = MetaDataEventType::META_DATA_FLUSH_COMPLETE;
  std::vector<Event> evs{ev};
  mCallback->postEvents(evs, isWakeUpSensor());
  sendAdditionalInfoReport();

  return Result::OK;
}

void Sensor::addDirectChannel(int32_t channelHandle, int64_t samplingPeriodNs) {
  ALOGD("Sensor addDirectChannel %s %d %ld", mSensorInfo.name.c_str(), channelHandle, samplingPeriodNs);

  if (samplingPeriodNs == 0) {
    {
      std::unique_lock<std::mutex> lock(mRunMutex);
      mDirectChannels[channelHandle] = {false, 0};
    }
    stopDirectChannel(channelHandle);
    return;
  }

  std::unique_lock<std::mutex> lock(mRunMutex);

  mDirectChannels[channelHandle] = {true, samplingPeriodNs};

  mSamplingPeriodNs = std::numeric_limits<int64_t>::max();
  for (const auto& [_, channel] : mDirectChannels) {
    if (channel.enabled && (channel.samplingPeriodNs < mSamplingPeriodNs)) {
      mSamplingPeriodNs = channel.samplingPeriodNs;
    }
  }

  mIsEnabled = true;
  mSensor->batch(mSamplingPeriodNs, 0);
  mSensor->activate(true);
  mWaitCV.notify_all();
}

void Sensor::stopDirectChannel(int32_t channelHandle) {
  ALOGD("Sensor stopDirectChannel %s %d", mSensorInfo.name.c_str(), channelHandle);

  std::unique_lock<std::mutex> lock(mRunMutex);

  auto itRateNs = mDirectChannels.find(channelHandle);
  if (itRateNs != mDirectChannels.end()) {
    itRateNs->second.enabled = false;
    itRateNs->second.samplingPeriodNs = 0;
  }

  bool anyChannelEnabled = false;
  for (const auto& [_, channel] : mDirectChannels) {
    if (channel.enabled) {
      anyChannelEnabled = true;
      break;
    }
  }

  if (!anyChannelEnabled) {
    mIsEnabled = false;
    mSensor->activate(false);
    mWaitCV.notify_all();
  }
}

void Sensor::removeDirectChannel(int32_t channelHandle) {
  ALOGD("Sensor removeDirectChannel %s %d", mSensorInfo.name.c_str(), channelHandle);

  std::unique_lock<std::mutex> lock(mRunMutex);

  auto itEnabled = mDirectChannels.find(channelHandle);
  if (itEnabled != mDirectChannels.end()) {
    mDirectChannels.erase(itEnabled);
    if (mDirectChannels.empty()) {
      mIsEnabled = false;
      mSensor->activate(false);
      mWaitCV.notify_all();
    }
  }
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
        if (!mDirectChannels.empty()) {
          mCallback->writeToDirectBuffer(readEvents(), mSamplingPeriodNs);
        } else {
          mCallback->postEvents(readEvents(), isWakeUpSensor());
        }
      }
      clock_gettime(CLOCK_BOOTTIME, &curTime);
      now = (curTime.tv_sec * kNanosecondsInSeconds) + curTime.tv_nsec;
      mWaitCV.wait_for(runLock, std::chrono::nanoseconds(nextSampleTime - now));
    }
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

bool areAlmostEqual(float a, float b, float epsilon = 1e-5) { return std::fabs(a - b) < epsilon; }

std::vector<Event> Sensor::readEvents() {
  std::vector<Event> events;
  const auto& values = mSensor->readSensorValues();
  const size_t xyzLength = 3;
  static float lastTemperature = 0;

  for (const auto& value : values) {
    Event event{};
    event.sensorHandle = mSensorInfo.sensorHandle;
    event.sensorType = mSensorInfo.type;
    event.timestamp = value.timestamp;

    if (mSensorInfo.type == SensorType::AMBIENT_TEMPERATURE) {
      if (!areAlmostEqual(value.data[0], lastTemperature)) {
        event.u.scalar = value.data[0];
        lastTemperature = value.data[0];
        // ALOGD("Temperature: %f, timestamp: %zu", event.u.scalar, static_cast<size_t>(event.timestamp));
      }
    } else {
      if (value.data.size() == xyzLength) {
        event.u.vec3.x = value.data[0];
        event.u.vec3.y = value.data[1];
        event.u.vec3.z = value.data[2];
        event.u.vec3.status = SensorStatus::ACCURACY_HIGH;
        // ALOGD("%f, %f, %f, %zu", event.u.vec3.x, event.u.vec3.y, event.u.vec3.z,
        // static_cast<size_t>(event.timestamp));
      } else {
        ALOGE("Read failed: %zu", static_cast<size_t>(value.data.size()));
      }
    }
    events.push_back(event);
  }

  return events;
}

}  // namespace implementation
}  // namespace subhal
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
