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

#include "SensorsSubHal.h"

#include <log/log.h>

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace subhal {
namespace implementation {

using ::android::hardware::Void;
using ::android::hardware::sensors::V1_0::OperationMode;
using ::android::hardware::sensors::V1_0::RateLevel;
using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V1_0::SharedMemInfo;
using ::android::hardware::sensors::V2_0::SensorTimeout;
using ::android::hardware::sensors::V2_0::WakeLockQueueFlagBits;
using ::android::hardware::sensors::V2_0::implementation::ScopedWakelock;
using ::android::hardware::sensors::V2_1::Event;

ISensorsSubHalBase::ISensorsSubHalBase() : mCallback(nullptr) {}

// Methods from ::android::hardware::sensors::V2_0::ISensors follow.
Return<void> ISensorsSubHalBase::getSensorsList(V2_1::ISensors::getSensorsList_2_1_cb _hidl_cb) {
  std::vector<SensorInfo> sensors;
  for (const auto& sensor : mSensors) {
    sensors.push_back(sensor.second->getSensorInfo());
  }

  _hidl_cb(sensors);
  return Void();
}

Return<Result> ISensorsSubHalBase::setOperationMode(OperationMode mode) {
  if (mode == OperationMode::NORMAL) {
    return Result::OK;
  } else {
    return Result::BAD_VALUE;
  }
}

Return<Result> ISensorsSubHalBase::activate(int32_t sensorHandle, bool enabled) {
  auto sensor = mSensors.find(sensorHandle);
  if (sensor != mSensors.end()) {
    sensor->second->activate(enabled);
    return Result::OK;
  }
  return Result::BAD_VALUE;
}

Return<Result> ISensorsSubHalBase::batch(int32_t sensorHandle, int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
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

void ISensorsSubHalBase::AddSensors() {
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
    sensorInfo.flags = 0;
    sensorInfo.minDelay = data.minDelayUs;
    sensorInfo.maxDelay = data.maxDelayUs;
    sensorInfo.power = data.power;
    sensorInfo.maxRange = data.range;
    sensorInfo.resolution = data.resolution;

    std::shared_ptr<Sensor> halSensor = std::make_shared<Sensor>(this /* callback */, sensorInfo, sensor);
    mSensors[halSensor->getSensorInfo().sensorHandle] = halSensor;
    ALOGD("AddSensor[%d] %s", halSensor->getSensorInfo().sensorHandle, halSensor->getSensorInfo().name.c_str());
  }
}

Return<Result> ISensorsSubHalBase::flush(int32_t sensorHandle) {
  auto sensor = mSensors.find(sensorHandle);
  if (sensor != mSensors.end()) {
    return sensor->second->flush();
  }
  return Result::BAD_VALUE;
}

Return<Result> ISensorsSubHalBase::injectSensorData(const Event& event) {
  auto sensor = mSensors.find(event.sensorHandle);
  if (sensor != mSensors.end()) {
    return sensor->second->injectEvent(event);
  }

  return Result::BAD_VALUE;
}

Return<void> ISensorsSubHalBase::registerDirectChannel(const SharedMemInfo& /* mem */,
                                                       V2_0::ISensors::registerDirectChannel_cb _hidl_cb) {
  _hidl_cb(Result::INVALID_OPERATION, -1 /* channelHandle */);
  return Return<void>();
}

Return<Result> ISensorsSubHalBase::unregisterDirectChannel(int32_t /* channelHandle */) {
  return Result::INVALID_OPERATION;
}

Return<void> ISensorsSubHalBase::configDirectReport(int32_t /* sensorHandle */, int32_t /* channelHandle */,
                                                    RateLevel /* rate */,
                                                    V2_0::ISensors::configDirectReport_cb _hidl_cb) {
  _hidl_cb(Result::INVALID_OPERATION, 0 /* reportToken */);
  return Return<void>();
}

Return<void> ISensorsSubHalBase::debug(const hidl_handle& fd, const hidl_vec<hidl_string>& args) {
  if (fd.getNativeHandle() == nullptr || fd->numFds < 1) {
    ALOGE("%s: missing fd for writing", __FUNCTION__);
    return Void();
  }

  FILE* out = fdopen(dup(fd->data[0]), "w");

  if (args.size() != 0) {
    fprintf(out,
            "Note: sub-HAL %s currently does not support args. Input arguments are "
            "ignored.\n",
            getName().c_str());
  }

  std::ostringstream stream;
  stream << "Available sensors:" << std::endl;
  for (auto sensor : mSensors) {
    SensorInfo info = sensor.second->getSensorInfo();
    stream << "Name: " << info.name << std::endl;
    stream << "Min delay: " << info.minDelay << std::endl;
    stream << "Flags: " << info.flags << std::endl;
  }
  stream << std::endl;

  fprintf(out, "%s", stream.str().c_str());

  fclose(out);
  return Return<void>();
}

Return<Result> ISensorsSubHalBase::initialize(std::unique_ptr<IHalProxyCallbackWrapperBase>& halProxyCallback) {
  mCallback = std::move(halProxyCallback);
  setOperationMode(OperationMode::NORMAL);
  return Result::OK;
}

void ISensorsSubHalBase::postEvents(const std::vector<Event>& events, bool wakeup) {
  ScopedWakelock wakelock = mCallback->createScopedWakelock(wakeup);
  mCallback->postEvents(events, std::move(wakelock));
}

}  // namespace implementation
}  // namespace subhal
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
