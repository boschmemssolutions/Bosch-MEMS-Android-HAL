/*
 * Copyright (C) 2019 The Android Open Source Project
 * Copyright (C) 2024-2025 Robert Bosch GmbH
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

#include <android/hardware/sensors/2.1/types.h>

#include <map>
#include <memory>
#include <mutex>

#include "ISensorHal.h"

using ::android::hardware::sensors::V1_0::AdditionalInfo;
using ::android::hardware::sensors::V1_0::AdditionalInfoType;
using ::android::hardware::sensors::V1_0::EventPayload;
using ::android::hardware::sensors::V1_0::OperationMode;
using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V2_1::Event;
using ::android::hardware::sensors::V2_1::SensorInfo;
using ::android::hardware::sensors::V2_1::SensorType;

namespace android {
namespace hardware {
namespace sensors {
namespace V2_1 {
namespace subhal {
namespace implementation {

class ISensorsEventCallback {
public:
  virtual ~ISensorsEventCallback(){};
  virtual void postEvents(const std::vector<Event>& events, bool wakeup) = 0;
  virtual void writeToDirectBuffer(const std::vector<Event>& events) = 0;
};

class Sensor : public bosch::sensors::ISensorValues, public std::enable_shared_from_this<Sensor> {
public:
  Sensor(ISensorsEventCallback* callback, const SensorInfo& sensorInfo,
         std::shared_ptr<bosch::sensors::ISensorHal> sensor);
  ~Sensor();

  void update(const std::vector<bosch::sensors::SensorValues>& values) override;

  const SensorInfo& getSensorInfo() const;
  void batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs);
  void activate(bool enable);
  bool isEnabled();
  Result flush();

  bool supportsDataInjection() const;
  Result injectEvent(const Event& event);

  void addDirectChannel(int32_t channelHandle, int64_t samplingPeriodNs);
  void stopDirectChannel(int32_t channelHandle);
  void removeDirectChannel(int32_t channelHandle);

private:
  Result getSensorPlacement(std::vector<AdditionalInfo>& additionalInfoFrames);
  Result getSensorTemperature(std::vector<AdditionalInfo>& additionalInfoFrames);
  void sendAdditionalInfoReport();

  bool isWakeUpSensor();

  void updateDirectChannel();

  bool mIsEnabled{false};
  bool mDirectChannelEnabled{false};
  int64_t mFlushRequestCount{0};
  int64_t mSamplingPeriodNs{0};
  int64_t mDirectChannelRateNs{0};
  int64_t mNextSampleTimeNs{std::numeric_limits<int64_t>::max()};
  int64_t mNextDirectChannelNs{std::numeric_limits<int64_t>::max()};

  struct DirectChannel {
    bool enabled;
    int64_t samplingPeriodNs;
  };
  std::map<int32_t, DirectChannel> mDirectChannels{};

  SensorInfo mSensorInfo;

  ISensorsEventCallback* mCallback;
  std::shared_ptr<bosch::sensors::ISensorHal> mSensor;

  std::mutex mMutex;
};

}  // namespace implementation
}  // namespace subhal
}  // namespace V2_1
}  // namespace sensors
}  // namespace hardware
}  // namespace android
