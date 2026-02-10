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

#pragma once

#include <aidl/android/hardware/sensors/BnSensors.h>

#include <map>

#include "ISensorHal.h"

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {

class ISensorsEventCallback {
public:
  using Event = ::aidl::android::hardware::sensors::Event;

  virtual ~ISensorsEventCallback(){};
  virtual void postEvents(const std::vector<Event>& events, bool wakeup) = 0;
  virtual void writeToDirectBuffer(const std::vector<Event>& events) = 0;
};

class Sensor : public bosch::sensors::ISensorValues, public std::enable_shared_from_this<Sensor> {
public:
  using OperationMode = ::aidl::android::hardware::sensors::ISensors::OperationMode;
  using Event = ::aidl::android::hardware::sensors::Event;
  using EventPayload = ::aidl::android::hardware::sensors::Event::EventPayload;
  using SensorInfo = ::aidl::android::hardware::sensors::SensorInfo;
  using SensorType = ::aidl::android::hardware::sensors::SensorType;
  using MetaDataEventType = ::aidl::android::hardware::sensors::Event::EventPayload::MetaData::MetaDataEventType;
  using AdditionalInfo = ::aidl::android::hardware::sensors::AdditionalInfo;
  using AdditionalInfoType = ::aidl::android::hardware::sensors::AdditionalInfo::AdditionalInfoType;

  Sensor(ISensorsEventCallback* callback, const SensorInfo& sensorInfo,
         std::shared_ptr<bosch::sensors::ISensorHal> sensor);
  ~Sensor();

  void update(const std::vector<bosch::sensors::SensorValues>& values) override;

  const SensorInfo& getSensorInfo() const;
  void batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs);
  void activate(bool enable);
  bool isEnabled();
  ndk::ScopedAStatus flush();

  ndk::ScopedAStatus setOperationMode(OperationMode mode);
  bool supportsDataInjection() const;
  ndk::ScopedAStatus injectEvent(const Event& event);

  void addDirectChannel(int32_t channelHandle, int64_t samplingPeriodNs);
  void stopDirectChannel(int32_t channelHandle);
  void removeDirectChannel(int32_t channelHandle);

private:
  ndk::ScopedAStatus getSensorPlacement(std::vector<AdditionalInfo>& additionalInfoFrames);
  ndk::ScopedAStatus getSensorTemperature(std::vector<AdditionalInfo>& additionalInfoFrames);
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

}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
