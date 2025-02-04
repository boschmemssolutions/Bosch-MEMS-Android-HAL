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
#include <utils/SystemClock.h>

#include <cmath>
#include <iostream>

using ::ndk::ScopedAStatus;

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

using ::bosch::sensor::hal::configuration::V1_0::Configuration;
using ::bosch::sensor::hal::configuration::V1_0::Location;
using ::bosch::sensor::hal::configuration::V1_0::Orientation;

ndk::ScopedAStatus Sensor::setSensorPlacementData(AdditionalInfo* sensorPlacement, int index, float value) {
  if (!sensorPlacement) return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  int arraySize = mAdditionalInfoValues.values.size();
  if (index < 0 || index >= arraySize) ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  mAdditionalInfoValues.values[index] = value;
  sensorPlacement->payload.set<AdditionalInfo::AdditionalInfoPayload::dataFloat>(mAdditionalInfoValues);
  return ScopedAStatus::ok();
}

std::optional<std::vector<Orientation>> Sensor::getOrientation() {
  if (!mConfig) return std::nullopt;
  if (mConfig->empty()) return std::nullopt;
  Configuration& sensorCfg = mConfig.value()[0];
  return sensorCfg.getOrientation();
}

std::optional<std::vector<Location>> Sensor::getLocation() {
  if (!mConfig) return std::nullopt;
  if (mConfig->empty()) return std::nullopt;
  Configuration& sensorCfg = mConfig.value()[0];
  return sensorCfg.getLocation();
}

ndk::ScopedAStatus Sensor::getSensorPlacement(std::vector<AdditionalInfo>& additionalInfoFrames) {
  AdditionalInfo sensorPlacement;
  auto sensorLocationList = getLocation();
  if (!sensorLocationList) return ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
  if (sensorLocationList->empty()) return ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
  auto sensorOrientationList = getOrientation();
  if (!sensorOrientationList) return ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
  if (sensorOrientationList->empty()) return ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
  sensorPlacement.type = AdditionalInfoType::AINFO_SENSOR_PLACEMENT;
  sensorPlacement.serial = 0;
  memset(&mAdditionalInfoValues.values, 0, sizeof(mAdditionalInfoValues.values));
  Location& sensorLocation = (*sensorLocationList)[0];
  // SensorPlacementData is given as a 3x4 matrix consisting of a 3x3 rotation matrix (R)
  // concatenated with a 3x1 location vector (t) in row major order. Example: This raw buffer:
  // {x1,y1,z1,l1,x2,y2,z2,l2,x3,y3,z3,l3} corresponds to the following 3x4 matrix:
  //  x1 y1 z1 l1
  //  x2 y2 z2 l2
  //  x3 y3 z3 l3
  // LOCATION_X_IDX,LOCATION_Y_IDX,LOCATION_Z_IDX corresponds to the indexes of the location
  // vector (l1,l2,l3) in the raw buffer.
  ndk::ScopedAStatus ret = setSensorPlacementData(&sensorPlacement, Sensor::LOCATION_X_IDX, sensorLocation.getX());
  if (!ret.isOk()) return ret;
  ret = setSensorPlacementData(&sensorPlacement, Sensor::LOCATION_Y_IDX, sensorLocation.getY());
  if (!ret.isOk()) return ret;
  ret = setSensorPlacementData(&sensorPlacement, Sensor::LOCATION_Z_IDX, sensorLocation.getZ());
  if (!ret.isOk()) return ret;
  Orientation& sensorOrientation = (*sensorOrientationList)[0];
  if (sensorOrientation.getRotate()) {
    // If the HAL is already rotating the sensor orientation to align with the Android
    // Coordinate system, then the sensor rotation matrix will be an identity matrix
    // ROTATION_X_IDX, ROTATION_Y_IDX, ROTATION_Z_IDX corresponds to indexes of the
    // (x1,y1,z1) in the raw buffer.
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_X_IDX + 0, 1);
    if (!ret.isOk()) return ret;
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_Y_IDX + 4, 1);
    if (!ret.isOk()) return ret;
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_Z_IDX + 8, 1);
    if (!ret.isOk()) return ret;
  } else {
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_X_IDX + 4 * sensorOrientation.getFirstX()->getMap(),
                                 sensorOrientation.getFirstX()->getNegate() ? -1 : 1);
    if (!ret.isOk()) return ret;
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_Y_IDX + 4 * sensorOrientation.getFirstY()->getMap(),
                                 sensorOrientation.getFirstY()->getNegate() ? -1 : 1);
    if (!ret.isOk()) return ret;
    ret = setSensorPlacementData(&sensorPlacement, Sensor::ROTATION_Z_IDX + 4 * sensorOrientation.getFirstZ()->getMap(),
                                 sensorOrientation.getFirstZ()->getNegate() ? -1 : 1);
    if (!ret.isOk()) return ret;
  }

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
               std::shared_ptr<bosch::sensors::ISensorHal> sensor,
               const std::optional<std::vector<Configuration>>& config)
  : mIsEnabled(false),
    mDirectChannelEnabled(false),
    mSensorInfo(sensorInfo),
    mStopThread(false),
    mCallback(callback),
    mSensor(sensor),
    mConfig(config) {
  mRunThread = std::thread(startThread, this);
  mSamplingPeriodNs = sensorInfo.minDelayUs * 1000LL;
  mDirectChannelRateNs = std::numeric_limits<int64_t>::max();
  mNextSampleTimeNs = std::numeric_limits<int64_t>::max();
  mNextDirectChannelNs = std::numeric_limits<int64_t>::max();
}

Sensor::~Sensor() {
  std::unique_lock<std::mutex> lock(mRunMutex);
  mStopThread = true;
  mIsEnabled = false;
  mDirectChannelEnabled = false;
  mWaitCV.notify_all();
  lock.release();
  mRunThread.join();
}

const SensorInfo& Sensor::getSensorInfo() const { return mSensorInfo; }

void Sensor::batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
  ALOGD("Sensor batch %s %zu %zu", mSensorInfo.name.c_str(), static_cast<size_t>(samplingPeriodNs),
        static_cast<size_t>(maxReportLatencyNs));

  std::unique_lock<std::mutex> lock(mRunMutex);
  if (samplingPeriodNs < mSensorInfo.minDelayUs * 1000LL) {
    samplingPeriodNs = mSensorInfo.minDelayUs * 1000LL;
  } else if (samplingPeriodNs > mSensorInfo.maxDelayUs * 1000LL) {
    samplingPeriodNs = mSensorInfo.maxDelayUs * 1000LL;
  }

  if (samplingPeriodNs < mDirectChannelRateNs) {
    mSensor->batch(samplingPeriodNs, maxReportLatencyNs);
  }

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

  std::unique_lock<std::mutex> lock(mRunMutex);
  if (mIsEnabled != enable) {
    mIsEnabled = enable;
    mNextSampleTimeNs = mIsEnabled ? 0 : std::numeric_limits<int64_t>::max();
    mWaitCV.notify_all();
    mSensor->activate(enable);
    if (enable) sendAdditionalInfoReport();
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
  sendAdditionalInfoReport();

  return ScopedAStatus::ok();
}

void Sensor::addDirectChannel(int32_t channelHandle, int64_t samplingPeriodNs) {
  ALOGD("Sensor addDirectChannel %s %d %zu", mSensorInfo.name.c_str(), channelHandle,
        static_cast<size_t>(samplingPeriodNs));

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
  updateDirectChannel();
}

void Sensor::stopDirectChannel(int32_t channelHandle) {
  ALOGD("Sensor stopDirectChannel %s %d", mSensorInfo.name.c_str(), channelHandle);

  std::unique_lock<std::mutex> lock(mRunMutex);

  auto itRateNs = mDirectChannels.find(channelHandle);
  if (itRateNs != mDirectChannels.end()) {
    itRateNs->second.enabled = false;
    itRateNs->second.samplingPeriodNs = 0;
  }
  updateDirectChannel();
}

void Sensor::removeDirectChannel(int32_t channelHandle) {
  ALOGD("Sensor removeDirectChannel %s %d", mSensorInfo.name.c_str(), channelHandle);

  std::unique_lock<std::mutex> lock(mRunMutex);

  auto itEnabled = mDirectChannels.find(channelHandle);
  if (itEnabled != mDirectChannels.end()) {
    mDirectChannels.erase(itEnabled);
  }
  updateDirectChannel();
}

void Sensor::updateDirectChannel() {
  int64_t directChannelRateNs = std::numeric_limits<int64_t>::max();
  for (const auto& [_, channel] : mDirectChannels) {
    if (channel.enabled && (channel.samplingPeriodNs < directChannelRateNs)) {
      directChannelRateNs = channel.samplingPeriodNs;
    }
  }
  if (mDirectChannelRateNs != directChannelRateNs) {
    mDirectChannelRateNs = directChannelRateNs;

    if (mDirectChannelRateNs < mSamplingPeriodNs) {
      mSensor->batch(mDirectChannelRateNs, 0);
    }
  }

  bool anyChannelEnabled = false;
  for (const auto& [_, channel] : mDirectChannels) {
    if (channel.enabled) {
      anyChannelEnabled = true;
      break;
    }
  }
  if (mDirectChannelEnabled != anyChannelEnabled) {
    mDirectChannelEnabled = anyChannelEnabled;
    mNextDirectChannelNs = mDirectChannelEnabled ? 0 : std::numeric_limits<int64_t>::max();
    if (!mIsEnabled) {
      mSensor->activate(mDirectChannelEnabled);
    }
  }

  mWaitCV.notify_all();
}

void Sensor::startThread(Sensor* sensor) { sensor->run(); }

void Sensor::run() {
  std::unique_lock<std::mutex> runLock(mRunMutex);

  while (!mStopThread) {
    if (!mIsEnabled && !mDirectChannelEnabled) {
      mWaitCV.wait(runLock, [&] { return (mIsEnabled || mDirectChannelEnabled || mStopThread); });
    } else {
      int64_t currentTime = ::android::elapsedRealtimeNano();
      std::vector<Event> events = readEvents();
      if (mDirectChannelEnabled) {
        if (currentTime >= mNextDirectChannelNs) {
          mNextDirectChannelNs = currentTime + mDirectChannelRateNs * bosch::sensors::POLL_TIME_REDUCTION_FACTOR;
          mCallback->writeToDirectBuffer(events, mDirectChannelRateNs);
        }
      }
      if (mIsEnabled) {
        if (currentTime >= mNextSampleTimeNs) {
          mNextSampleTimeNs = currentTime + mSamplingPeriodNs * bosch::sensors::POLL_TIME_REDUCTION_FACTOR;
          mCallback->postEvents(events, isWakeUpSensor());
        }
      }
      currentTime = ::android::elapsedRealtimeNano();
      int64_t waitTime = std::min(mNextSampleTimeNs, mNextDirectChannelNs) - currentTime;
      if (waitTime < 1000000) waitTime = 1000000;
      mWaitCV.wait_for(runLock, std::chrono::nanoseconds(waitTime));
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

bool areAlmostEqual(float a, float b, float epsilon = 1e-5) { return std::fabs(a - b) < epsilon; }

float gyroUncalibratedFix(const SensorInfo& mSensorInfo) {
  std::string sensorName = mSensorInfo.name;
  std::string smi230Keyword("SMI230 BOSCH");
  if (sensorName.find(smi230Keyword) != std::string::npos)
    return mSensorInfo.resolution;
  else
    return 0;
}

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
        lastTemperature = value.data[0];

        float scalar = value.data[0];
        // ALOGD("Temperature: %f", scalar);
        event.payload.set<EventPayload::Tag::scalar>(scalar);
      }
    } else if (mSensorInfo.type == SensorType::GYROSCOPE_UNCALIBRATED) {
      if (value.data.size() == xyzLength) {
        EventPayload::Uncal uncal = {
          .x = value.data[0] + gyroUncalibratedFix(mSensorInfo),
          .y = value.data[1] + gyroUncalibratedFix(mSensorInfo),
          .z = value.data[2] + gyroUncalibratedFix(mSensorInfo),
          .xBias = 0,
          .yBias = 0,
          .zBias = 0,
        };
        event.payload.set<EventPayload::Tag::uncal>(uncal);
      } else {
        ALOGE("Read failed: %zu", static_cast<size_t>(value.data.size()));
      }
    } else if (mSensorInfo.type == SensorType::ACCELEROMETER_UNCALIBRATED) {
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
      }
    }
    events.push_back(event);
  }

  return events;
}

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
