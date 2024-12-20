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

#include <log/log.h>

#include <vector>

#include "DirectChannel.h"
#include "IHalProxyCallbackWrapper.h"
#include "Sensor.h"
#include "SensorList.h"
#include "V2_0/SubHal.h"
#include "V2_1/SubHal.h"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace subhal {
namespace implementation {

using ::android::hardware::sensors::V1_0::OperationMode;
using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V2_1::SensorType;

class ISensorsSubHalBase : public ISensorsEventCallback {
protected:
  using Event = ::android::hardware::sensors::V2_1::Event;
  using RateLevel = ::android::hardware::sensors::V1_0::RateLevel;
  using SharedMemInfo = ::android::hardware::sensors::V1_0::SharedMemInfo;

public:
  ISensorsSubHalBase();

  Return<void> getSensorsList(V2_1::ISensors::getSensorsList_2_1_cb _hidl_cb);
  Return<Result> injectSensorData(const Event& event);
  Return<Result> initialize(std::unique_ptr<IHalProxyCallbackWrapperBase>& halProxyCallback);

  // Methods from ::android::hardware::sensors::V2_0::ISensors follow.
  virtual Return<Result> setOperationMode(OperationMode mode);

  OperationMode getOperationMode() const { return mCurrentOperationMode; }

  Return<Result> activate(int32_t sensorHandle, bool enabled);

  Return<Result> batch(int32_t sensorHandle, int64_t samplingPeriodNs, int64_t maxReportLatencyNs);

  Return<Result> flush(int32_t sensorHandle);

  Return<void> registerDirectChannel(const SharedMemInfo& mem, V2_0::ISensors::registerDirectChannel_cb _hidl_cb);

  Return<Result> unregisterDirectChannel(int32_t channelHandle);

  Return<void> configDirectReport(int32_t sensorHandle, int32_t channelHandle, RateLevel rate,
                                  V2_0::ISensors::configDirectReport_cb _hidl_cb);

  Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& args);

  // Methods from
  // ::android::hardware::sensors::V2_0::implementation::ISensorsSubHal follow.
  const std::string getName() { return "BoschSubHal"; }

  // Method from ISensorsEventCallback.
  void postEvents(const std::vector<Event>& events, bool wakeup) override;
  void writeToDirectBuffer(const std::vector<Event>& events, int64_t samplingPeriodNs) override;

protected:
  void AddSensors();
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
  int32_t mNextHandle = 1;

  /**
   * Callback used to communicate to the HalProxy when dynamic sensors are
   * connected / disconnected, sensor events need to be sent to the framework,
   * and when a wakelock should be acquired.
   */
  std::unique_ptr<IHalProxyCallbackWrapperBase> mCallback;

private:
  /**
   * The current operation mode of the multihal framework. Ensures that all
   * subhals are set to the same operation mode.
   */
  OperationMode mCurrentOperationMode = OperationMode::NORMAL;

  /**
   * A list of the available sensors
   */
  bosch::sensors::SensorList mSensorList;

  /**
   * Direct channel support
   */
  std::map<int32_t, std::unique_ptr<DirectChannelBase>> mDirectChannels;
  std::mutex mChannelMutex;
  int32_t mNextChannelHandle = 1;
};

template <class SubHalClass>
class SensorsSubHalBase : public ISensorsSubHalBase, public SubHalClass {
public:
  Return<Result> setOperationMode(OperationMode mode) override { return ISensorsSubHalBase::setOperationMode(mode); }

  Return<Result> activate(int32_t sensorHandle, bool enabled) override {
    return ISensorsSubHalBase::activate(sensorHandle, enabled);
  }

  Return<Result> batch(int32_t sensorHandle, int64_t samplingPeriodNs, int64_t maxReportLatencyNs) override {
    return ISensorsSubHalBase::batch(sensorHandle, samplingPeriodNs, maxReportLatencyNs);
  }

  Return<Result> flush(int32_t sensorHandle) override { return ISensorsSubHalBase::flush(sensorHandle); }

  Return<void> registerDirectChannel(const SharedMemInfo& mem,
                                     V2_0::ISensors::registerDirectChannel_cb _hidl_cb) override {
    return ISensorsSubHalBase::registerDirectChannel(mem, _hidl_cb);
  }

  Return<Result> unregisterDirectChannel(int32_t channelHandle) override {
    return ISensorsSubHalBase::unregisterDirectChannel(channelHandle);
  }

  Return<void> configDirectReport(int32_t sensorHandle, int32_t channelHandle, RateLevel rate,
                                  V2_0::ISensors::configDirectReport_cb _hidl_cb) override {
    return ISensorsSubHalBase::configDirectReport(sensorHandle, channelHandle, rate, _hidl_cb);
  }

  Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& args) override {
    return ISensorsSubHalBase::debug(fd, args);
  }

  const std::string getName() override { return ISensorsSubHalBase::getName(); }
};

class SensorsSubHalV2_0 : public SensorsSubHalBase<V2_0::implementation::ISensorsSubHal> {
public:
  virtual Return<void> getSensorsList(V2_0::ISensors::getSensorsList_cb _hidl_cb) override {
    return ISensorsSubHalBase::getSensorsList(
      [&](const auto& list) { _hidl_cb(V2_1::implementation::convertToOldSensorInfos(list)); });
  }

  Return<Result> injectSensorData(const V1_0::Event& event) override {
    return ISensorsSubHalBase::injectSensorData(V2_1::implementation::convertToNewEvent(event));
  }

  Return<Result> initialize(const sp<V2_0::implementation::IHalProxyCallback>& halProxyCallback) override {
    std::unique_ptr<IHalProxyCallbackWrapperBase> wrapper =
      std::make_unique<HalProxyCallbackWrapperV2_0>(halProxyCallback);
    return ISensorsSubHalBase::initialize(wrapper);
  }
};

class SensorsSubHalV2_1 : public SensorsSubHalBase<V2_1::implementation::ISensorsSubHal> {
public:
  Return<void> getSensorsList_2_1(V2_1::ISensors::getSensorsList_2_1_cb _hidl_cb) override {
    return ISensorsSubHalBase::getSensorsList(_hidl_cb);
  }

  Return<Result> injectSensorData_2_1(const V2_1::Event& event) override {
    return ISensorsSubHalBase::injectSensorData(event);
  }

  Return<Result> initialize(const sp<V2_1::implementation::IHalProxyCallback>& halProxyCallback) override {
    std::unique_ptr<IHalProxyCallbackWrapperBase> wrapper =
      std::make_unique<HalProxyCallbackWrapperV2_1>(halProxyCallback);
    return ISensorsSubHalBase::initialize(wrapper);
  }
};

}  // namespace implementation
}  // namespace subhal
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
