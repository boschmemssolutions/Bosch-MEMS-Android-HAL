/*
 * Copyright (C) 2023 Robert Bosch GmbH. All rights reserved.
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

#ifndef ANDROID_HARDWARE_SENSORS_V2_X_SENSOR_H
#define ANDROID_HARDWARE_SENSORS_V2_X_SENSOR_H

#include <android/hardware/sensors/1.0/types.h>
#include <android/hardware/sensors/2.1/types.h>

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ISensorHal.h"
#include "bosch_sensor_hal_configuration_V1_0.h"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_X {
namespace implementation {

class ISensorsEventCallback {
public:
  using Event = ::android::hardware::sensors::V2_1::Event;

  virtual ~ISensorsEventCallback(){};
  virtual void postEvents(const std::vector<Event>& events, bool wakeup) = 0;
  virtual void writeToDirectBuffer(const std::vector<Event>& events, int64_t samplingPeriodNs) = 0;
};

class Sensor {
public:
  using OperationMode = ::android::hardware::sensors::V1_0::OperationMode;
  using Result = ::android::hardware::sensors::V1_0::Result;
  using Event = ::android::hardware::sensors::V2_1::Event;
  using AdditionalInfo = ::android::hardware::sensors::V1_0::AdditionalInfo;
  using EventPayload = ::android::hardware::sensors::V1_0::EventPayload;
  using SensorInfo = ::android::hardware::sensors::V2_1::SensorInfo;
  using SensorType = ::android::hardware::sensors::V2_1::SensorType;
  using AdditionalInfoType = ::android::hardware::sensors::V1_0::AdditionalInfoType;
  using Location = ::bosch::sensor::hal::configuration::V1_0::Location;
  using Orientation = ::bosch::sensor::hal::configuration::V1_0::Orientation;
  using Configuration = ::bosch::sensor::hal::configuration::V1_0::Configuration;

  Sensor(ISensorsEventCallback* callback, const SensorInfo& sensorInfo,
         std::shared_ptr<bosch::sensors::ISensorHal> sensor, const std::optional<std::vector<Configuration>>& config);
  ~Sensor();

  const SensorInfo& getSensorInfo() const;
  void batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs);
  void activate(bool enable);
  bool isEnabled();
  Result flush();

  Result setOperationMode(OperationMode mode);
  bool supportsDataInjection() const;
  Result injectEvent(const Event& event);

  void addDirectChannel(int32_t channelHandle, int64_t samplingPeriodNs);
  void stopDirectChannel(int32_t channelHandle);
  void removeDirectChannel(int32_t channelHandle);
  void updateDirectChannel();

private:
  void run();
  std::vector<Event> readEvents();
  static void startThread(Sensor* sensor);
  Result getSensorPlacement(std::vector<AdditionalInfo>& additionalInfoFrames);
  Result getSensorTemperature(std::vector<AdditionalInfo>& additionalInfoFrames);
  void sendAdditionalInfoReport();

  bool isWakeUpSensor();

  static constexpr uint8_t LOCATION_X_IDX = 3;
  static constexpr uint8_t LOCATION_Y_IDX = 7;
  static constexpr uint8_t LOCATION_Z_IDX = 11;
  static constexpr uint8_t ROTATION_X_IDX = 0;
  static constexpr uint8_t ROTATION_Y_IDX = 1;
  static constexpr uint8_t ROTATION_Z_IDX = 2;

  bool mIsEnabled;
  bool mDirectChannelEnabled;
  int64_t mSamplingPeriodNs;
  int64_t mNextSampleTimeNs;
  int64_t mDirectChannelRateNs;
  int64_t mNextDirectChannelNs;
  SensorInfo mSensorInfo;

  struct DirectChannel {
    bool enabled;
    int64_t samplingPeriodNs;
  };
  std::map<int32_t, DirectChannel> mDirectChannels{};

  std::atomic_bool mStopThread;
  std::condition_variable mWaitCV;
  std::mutex mRunMutex;
  std::thread mRunThread;

  ISensorsEventCallback* mCallback;

  std::shared_ptr<bosch::sensors::ISensorHal> mSensor;
  std::optional<std::vector<Configuration>> mConfig;
};

}  // namespace implementation
}  // namespace V2_X
}  // namespace sensors
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_SENSORS_V2_X_SENSOR_H
