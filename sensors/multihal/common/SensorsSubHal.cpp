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
#include <sys/mman.h>

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

using Configuration = ::bosch::sensor::hal::configuration::V1_0::Configuration;
using SensorType = ::android::hardware::sensors::V2_1::SensorType;
using RateLevel = ::android::hardware::sensors::V1_0::RateLevel;

#define SENSOR_XML_CONFIG_FILE_NAME "sensor_hal_configuration.xml"
static const char* gSensorConfigLocationList[] = {"/odm/etc/sensors/", "/vendor/etc/sensors/"};
static const int gSensorConfigLocationListSize =
  (sizeof(gSensorConfigLocationList) / sizeof(gSensorConfigLocationList[0]));

#define MODULE_NAME "bosch-hal"

static std::optional<std::vector<Configuration>> getSensorConfiguration(
  const std::vector<::bosch::sensor::hal::configuration::V1_0::Sensor>& sensor_list, const std::string& name,
  SensorType type) {
  for (auto sensor : sensor_list) {
    if ((name.compare(sensor.getName()) == 0) && (type == (SensorType)sensor.getType())) {
      return sensor.getConfiguration();
    }
  }
  return std::nullopt;
}

static std::optional<std::vector<::bosch::sensor::hal::configuration::V1_0::Sensor>> readSensorsConfigFromXml() {
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
    std::shared_ptr<Sensor> halSensor = std::make_shared<Sensor>(this /* callback */, sensorInfo, sensor, sensorconfig);
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

Return<void> ISensorsSubHalBase::registerDirectChannel(const SharedMemInfo& mem,
                                                       V2_0::ISensors::registerDirectChannel_cb _hidl_cb) {
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

Return<Result> ISensorsSubHalBase::unregisterDirectChannel(int32_t channelHandle) {
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

Return<void> ISensorsSubHalBase::configDirectReport(int32_t sensorHandle, int32_t channelHandle, RateLevel rate,
                                                    V2_0::ISensors::configDirectReport_cb _hidl_cb) {
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

  for (auto& [channelHandle, channel] : mDirectChannels) {
    for (auto sensorHandle : channel->sensorHandles) {
      auto sensor = mSensors.find(sensorHandle);
      if (sensor != mSensors.end()) {
        sensor->second->removeDirectChannel(channelHandle);
      }
    }
  }
  mDirectChannels.clear();

  setOperationMode(OperationMode::NORMAL);
  return Result::OK;
}

void ISensorsSubHalBase::postEvents(const std::vector<Event>& events, bool wakeup) {
  ScopedWakelock wakelock = mCallback->createScopedWakelock(wakeup);
  mCallback->postEvents(events, std::move(wakelock));
}

void ISensorsSubHalBase::writeToDirectBuffer(const std::vector<Event>& events, int64_t samplingPeriodNs) {
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

}  // namespace implementation
}  // namespace subhal
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
