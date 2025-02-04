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

#include <android/hardware/sensors/2.1/ISensors.h>
#include <android/hardware/sensors/2.1/types.h>
#include <hidl/GtestPrinter.h>
#include <hidl/ServiceManagement.h>
#include <log/log.h>
#include <utils/SystemClock.h>

#include <algorithm>
#include <cinttypes>
#include <condition_variable>
#include <cstring>
#include <map>
#include <regex>
#include <unordered_map>
#include <vector>

#include "SensorsHidlEnvironmentV2_X.h"
#include "convertV2_1.h"
#include "sensors-vts-utils/SensorsHidlTestBase.h"
#include "sensors-vts-utils/SensorsTestSharedMemory.h"

/**
 * This file contains the core tests and test logic for both sensors HAL 2.0
 * and 2.1. To make it easier to share the code between both VTS test suites,
 * this is defined as a header so they can both include and use all pieces of
 * code.
 */

using ::android::sp;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::sensors::V1_0::MetaDataEventType;
using ::android::hardware::sensors::V1_0::OperationMode;
using ::android::hardware::sensors::V1_0::SensorsEventFormatOffset;
using ::android::hardware::sensors::V1_0::SensorStatus;
using ::android::hardware::sensors::V1_0::SharedMemType;
using ::android::hardware::sensors::V1_0::Vec3;
using ::android::hardware::sensors::V2_1::implementation::convertToOldSensorInfos;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;

using EventV1_0 = ::android::hardware::sensors::V1_0::Event;
using ISensorsType = ::android::hardware::sensors::V2_1::ISensors;
using SensorTypeVersion = ::android::hardware::sensors::V2_1::SensorType;
using EventType = ::android::hardware::sensors::V2_1::Event;
using SensorInfoType = ::android::hardware::sensors::V2_1::SensorInfo;
using SensorsHidlTestBaseV2_X = SensorsHidlTestBase<SensorTypeVersion, EventType, SensorInfoType>;

constexpr size_t kEventSize = static_cast<size_t>(SensorsEventFormatOffset::TOTAL_LENGTH);

class EventCallback : public IEventCallback<EventType> {
public:
  void reset() {
    mFlushMap.clear();
    mEventMap.clear();
  }

  void onEvent(const EventType& event) override {
    if (event.sensorType == SensorTypeVersion::META_DATA &&
        event.u.meta.what == MetaDataEventType::META_DATA_FLUSH_COMPLETE) {
      std::unique_lock<std::recursive_mutex> lock(mFlushMutex);
      mFlushMap[event.sensorHandle]++;
      mFlushCV.notify_all();
    } else if (event.sensorType != SensorTypeVersion::ADDITIONAL_INFO) {
      std::unique_lock<std::recursive_mutex> lock(mEventMutex);
      mEventMap[event.sensorHandle].push_back(event);
      mEventCV.notify_all();
    }
  }

  int32_t getFlushCount(int32_t sensorHandle) {
    std::unique_lock<std::recursive_mutex> lock(mFlushMutex);
    return mFlushMap[sensorHandle];
  }

  void waitForFlushEvents(const std::vector<SensorInfoType>& sensorsToWaitFor, int32_t numCallsToFlush,
                          milliseconds timeout) {
    std::unique_lock<std::recursive_mutex> lock(mFlushMutex);
    mFlushCV.wait_for(lock, timeout, [&] { return flushesReceived(sensorsToWaitFor, numCallsToFlush); });
  }

  const std::vector<EventType> getEvents(int32_t sensorHandle) {
    std::unique_lock<std::recursive_mutex> lock(mEventMutex);
    return mEventMap[sensorHandle];
  }

  void waitForEvents(const std::vector<SensorInfoType>& sensorsToWaitFor, milliseconds timeout) {
    std::unique_lock<std::recursive_mutex> lock(mEventMutex);
    mEventCV.wait_for(lock, timeout, [&] { return eventsReceived(sensorsToWaitFor); });
  }

protected:
  bool flushesReceived(const std::vector<SensorInfoType>& sensorsToWaitFor, int32_t numCallsToFlush) {
    for (const SensorInfoType& sensor : sensorsToWaitFor) {
      if (getFlushCount(sensor.sensorHandle) < numCallsToFlush) {
        return false;
      }
    }
    return true;
  }

  bool eventsReceived(const std::vector<SensorInfoType>& sensorsToWaitFor) {
    for (const SensorInfoType& sensor : sensorsToWaitFor) {
      if (getEvents(sensor.sensorHandle).size() == 0) {
        return false;
      }
    }
    return true;
  }

  std::map<int32_t, int32_t> mFlushMap;
  std::recursive_mutex mFlushMutex;
  std::condition_variable_any mFlushCV;

  std::map<int32_t, std::vector<EventType>> mEventMap;
  std::recursive_mutex mEventMutex;
  std::condition_variable_any mEventCV;
};

/**
 * Define the template specific versions of the static helper methods in
 * SensorsHidlTestBase used to test that hinge angle is exposed properly.
 */
template <>
SensorFlagBits expectedReportModeForType(::android::hardware::sensors::V2_1::SensorType type) {
  switch (type) {
    case ::android::hardware::sensors::V2_1::SensorType::HINGE_ANGLE:
      return SensorFlagBits::ON_CHANGE_MODE;
    default:
      return expectedReportModeForType(static_cast<::android::hardware::sensors::V1_0::SensorType>(type));
  }
}

template <>
void assertTypeMatchStringType(::android::hardware::sensors::V2_1::SensorType type, const hidl_string& stringType) {
  switch (type) {
    case (::android::hardware::sensors::V2_1::SensorType::HINGE_ANGLE):
      ASSERT_STREQ(SENSOR_STRING_TYPE_HINGE_ANGLE, stringType.c_str());
      break;
    default:
      assertTypeMatchStringType(static_cast<::android::hardware::sensors::V1_0::SensorType>(type), stringType);
      break;
  }
}

// The main test class for SENSORS HIDL HAL.
class SensorsHidlTest : public SensorsHidlTestBaseV2_X {
public:
  virtual void SetUp() override {
    mEnvironment = new SensorsHidlEnvironmentV2_X(GetParam());
    mEnvironment->SetUp();
    ALOGD("####################### %s #######################",
          ::testing::UnitTest::GetInstance()->current_test_info()->name());

    // Ensure that we have a valid environment before performing tests
    ASSERT_NE(getSensors(), nullptr);
  }

  virtual void TearDown() override { mEnvironment->TearDown(); }

protected:
  SensorInfoType defaultSensorByType(SensorTypeVersion type) override;
  std::vector<SensorInfoType> getSensorsList();
  // implementation wrapper

  Return<void> getSensorsList(ISensorsType::getSensorsList_cb _hidl_cb) override {
    return getSensors()->getSensorsList([&](const auto& list) { _hidl_cb(convertToOldSensorInfos(list)); });
  }

  Return<Result> activate(int32_t sensorHandle, bool enabled) override;

  Return<Result> batch(int32_t sensorHandle, int64_t samplingPeriodNs, int64_t maxReportLatencyNs) override {
    return getSensors()->batch(sensorHandle, samplingPeriodNs, maxReportLatencyNs);
  }

  Return<Result> flush(int32_t sensorHandle) override { return getSensors()->flush(sensorHandle); }

  Return<Result> injectSensorData(const EventType& event) override { return getSensors()->injectSensorData(event); }

  Return<void> registerDirectChannel(const SharedMemInfo& mem,
                                     ISensorsType::registerDirectChannel_cb _hidl_cb) override;

  Return<Result> unregisterDirectChannel(int32_t channelHandle) override {
    return getSensors()->unregisterDirectChannel(channelHandle);
  }

  Return<void> configDirectReport(int32_t sensorHandle, int32_t channelHandle, RateLevel rate,
                                  ISensorsType::configDirectReport_cb _hidl_cb) override {
    return getSensors()->configDirectReport(sensorHandle, channelHandle, rate, _hidl_cb);
  }

  inline sp<ISensorsWrapperBase>& getSensors() { return mEnvironment->mSensors; }

  SensorsVtsEnvironmentBase<EventType>* getEnvironment() override { return mEnvironment; }

  // Test helpers
  void runSingleFlushTest(const std::vector<SensorInfoType>& sensors, bool activateSensor, int32_t expectedFlushCount,
                          Result expectedResponse);
  void runFlushTest(const std::vector<SensorInfoType>& sensors, bool activateSensor, int32_t flushCalls,
                    int32_t expectedFlushCount, Result expectedResponse);

  // Helper functions
  void activateAllSensors(bool enable);
  std::vector<SensorInfoType> getNonOneShotSensors();
  std::vector<SensorInfoType> getNonOneShotAndNonSpecialSensors();
  std::vector<SensorInfoType> getNonOneShotAndNonOnChangeAndNonSpecialSensors();
  std::vector<SensorInfoType> getOneShotSensors();
  std::vector<SensorInfoType> getInjectEventSensors();
  int32_t getInvalidSensorHandle();
  bool getDirectChannelSensor(SensorInfoType* sensor, SharedMemType* memType, RateLevel* rate);
  void verifyDirectChannel(SharedMemType memType);
  void verifyRegisterDirectChannel(std::shared_ptr<SensorsTestSharedMemory<SensorTypeVersion, EventType>> mem,
                                   int32_t* directChannelHandle, bool supportsSharedMemType,
                                   bool supportsAnyDirectChannel);
  void verifyConfigure(const SensorInfoType& sensor, SharedMemType memType, int32_t directChannelHandle,
                       bool directChannelSupported);
  void verifyUnregisterDirectChannel(int32_t directChannelHandle, bool directChannelSupported);
  void checkRateLevel(const SensorInfoType& sensor, int32_t directChannelHandle, RateLevel rateLevel);
  void queryDirectChannelSupport(SharedMemType memType, bool* supportsSharedMemType, bool* supportsAnyDirectChannel);
  void checkVec3Sensor(SensorTypeVersion type, const Vec3& lowerLimit, const Vec3& upperLimit);

private:
  // Test environment for sensors HAL.
  SensorsHidlEnvironmentV2_X* mEnvironment;
};

Return<Result> SensorsHidlTest::activate(int32_t sensorHandle, bool enabled) {
  // If activating a sensor, add the handle in a set so that when test fails it can be turned off.
  // The handle is not removed when it is deactivating on purpose so that it is not necessary to
  // check the return value of deactivation. Deactivating a sensor more than once does not have
  // negative effect.
  if (enabled) {
    mSensorHandles.insert(sensorHandle);
  }
  return getSensors()->activate(sensorHandle, enabled);
}

Return<void> SensorsHidlTest::registerDirectChannel(const SharedMemInfo& mem, ISensors::registerDirectChannel_cb cb) {
  // If registeration of a channel succeeds, add the handle of channel to a set so that it can be
  // unregistered when test fails. Unregister a channel does not remove the handle on purpose.
  // Unregistering a channel more than once should not have negative effect.
  getSensors()->registerDirectChannel(mem, [&](auto result, auto channelHandle) {
    if (result == Result::OK) {
      mDirectChannelHandles.insert(channelHandle);
    }
    cb(result, channelHandle);
  });
  return Void();
}

SensorInfoType SensorsHidlTest::defaultSensorByType(SensorTypeVersion type) {
  SensorInfoType ret;

  ret.type = (SensorTypeVersion)-1;
  getSensors()->getSensorsList([&](const auto& list) {
    const size_t count = list.size();
    for (size_t i = 0; i < count; ++i) {
      if (list[i].type == type) {
        ret = list[i];
        return;
      }
    }
  });

  return ret;
}

std::vector<SensorInfoType> SensorsHidlTest::getSensorsList() {
  std::vector<SensorInfoType> ret;

  getSensors()->getSensorsList([&](const auto& list) {
    const size_t count = list.size();
    ret.reserve(list.size());
    for (size_t i = 0; i < count; ++i) {
      ret.push_back(list[i]);
    }
  });

  return ret;
}

std::vector<SensorInfoType> SensorsHidlTest::getNonOneShotSensors() {
  std::vector<SensorInfoType> sensors;
  for (const SensorInfoType& info : getSensorsList()) {
    if (extractReportMode(info.flags) != SensorFlagBits::ONE_SHOT_MODE) {
      sensors.push_back(info);
    }
  }
  return sensors;
}

std::vector<SensorInfoType> SensorsHidlTest::getNonOneShotAndNonSpecialSensors() {
  std::vector<SensorInfoType> sensors;
  for (const SensorInfoType& info : getSensorsList()) {
    SensorFlagBits reportMode = extractReportMode(info.flags);
    if (reportMode != SensorFlagBits::ONE_SHOT_MODE && reportMode != SensorFlagBits::SPECIAL_REPORTING_MODE) {
      sensors.push_back(info);
    }
  }
  return sensors;
}

std::vector<SensorInfoType> SensorsHidlTest::getNonOneShotAndNonOnChangeAndNonSpecialSensors() {
  std::vector<SensorInfoType> sensors;
  for (const SensorInfoType& info : getSensorsList()) {
    SensorFlagBits reportMode = extractReportMode(info.flags);
    if (reportMode != SensorFlagBits::ONE_SHOT_MODE && reportMode != SensorFlagBits::ON_CHANGE_MODE &&
        reportMode != SensorFlagBits::SPECIAL_REPORTING_MODE) {
      sensors.push_back(info);
    }
  }
  return sensors;
}

std::vector<SensorInfoType> SensorsHidlTest::getOneShotSensors() {
  std::vector<SensorInfoType> sensors;
  for (const SensorInfoType& info : getSensorsList()) {
    if (extractReportMode(info.flags) == SensorFlagBits::ONE_SHOT_MODE) {
      sensors.push_back(info);
    }
  }
  return sensors;
}

std::vector<SensorInfoType> SensorsHidlTest::getInjectEventSensors() {
  std::vector<SensorInfoType> sensors;
  for (const SensorInfoType& info : getSensorsList()) {
    if (info.flags & static_cast<uint32_t>(SensorFlagBits::DATA_INJECTION)) {
      sensors.push_back(info);
    }
  }
  return sensors;
}

int32_t SensorsHidlTest::getInvalidSensorHandle() {
  // Find a sensor handle that does not exist in the sensor list
  int32_t maxHandle = 0;
  for (const SensorInfoType& sensor : getSensorsList()) {
    maxHandle = std::max(maxHandle, sensor.sensorHandle);
  }
  return maxHandle + 42;
}

// Test if sensor list returned is valid
TEST_P(SensorsHidlTest, SensorListValid) {
  getSensors()->getSensorsList([&](const auto& list) {
    const size_t count = list.size();
    std::unordered_map<int32_t, std::vector<std::string>> sensorTypeNameMap;
    for (size_t i = 0; i < count; ++i) {
      const auto& s = list[i];
      SCOPED_TRACE(::testing::Message() << i << "/" << count << ": "
                                        << " handle=0x" << std::hex << std::setw(8) << std::setfill('0')
                                        << s.sensorHandle << std::dec << " type=" << static_cast<int>(s.type)
                                        << " name=" << s.name);

      // Test type string non-empty only for private sensor types.
      if (s.type >= SensorTypeVersion::DEVICE_PRIVATE_BASE) {
        EXPECT_FALSE(s.typeAsString.empty());
      } else if (!s.typeAsString.empty()) {
        // Test type string matches framework string if specified for non-private types.
        EXPECT_NO_FATAL_FAILURE(assertTypeMatchStringType(s.type, s.typeAsString));
      }

      // Test if all sensor has name and vendor
      EXPECT_FALSE(s.name.empty());
      EXPECT_FALSE(s.vendor.empty());

      // Make sure that sensors of the same type have a unique name.
      std::vector<std::string>& v = sensorTypeNameMap[static_cast<int32_t>(s.type)];
      bool isUniqueName = std::find(v.begin(), v.end(), s.name) == v.end();
      EXPECT_TRUE(isUniqueName) << "Duplicate sensor Name: " << s.name;
      if (isUniqueName) {
        v.push_back(s.name);
      }

      // Test power > 0, maxRange > 0
      EXPECT_LE(0, s.power);
      EXPECT_LT(0, s.maxRange);

      // Info type, should have no sensor
      EXPECT_FALSE(s.type == SensorTypeVersion::ADDITIONAL_INFO || s.type == SensorTypeVersion::META_DATA);

      // Test fifoMax >= fifoReserved
      EXPECT_GE(s.fifoMaxEventCount, s.fifoReservedEventCount)
        << "max=" << s.fifoMaxEventCount << " reserved=" << s.fifoReservedEventCount;

      // Test Reporting mode valid
      EXPECT_NO_FATAL_FAILURE(assertTypeMatchReportMode(s.type, extractReportMode(s.flags)));

      // Test min max are in the right order
      EXPECT_LE(s.minDelay, s.maxDelay);
      // Test min/max delay matches reporting mode
      EXPECT_NO_FATAL_FAILURE(assertDelayMatchReportMode(s.minDelay, s.maxDelay, extractReportMode(s.flags)));
    }
  });
}

// Test if sensor list contains Bosch sensor
TEST_P(SensorsHidlTest, SensorListContainsBoschSensor) {
  getSensors()->getSensorsList([&](const auto& list) {
    for (const auto& sensor : list) {
      EXPECT_STREQ(sensor.vendor.c_str(), "Robert Bosch GmbH");
    }
  });
}

// Test that SetOperationMode returns the expected value
TEST_P(SensorsHidlTest, SetOperationMode) {
  std::vector<SensorInfoType> sensors = getInjectEventSensors();
  if (getInjectEventSensors().size() > 0) {
    ASSERT_EQ(Result::OK, getSensors()->setOperationMode(OperationMode::NORMAL));
    ASSERT_EQ(Result::OK, getSensors()->setOperationMode(OperationMode::DATA_INJECTION));
    ASSERT_EQ(Result::OK, getSensors()->setOperationMode(OperationMode::NORMAL));
  } else {
    ASSERT_EQ(Result::BAD_VALUE, getSensors()->setOperationMode(OperationMode::DATA_INJECTION));
  }
}

// Test that an injected event is written back to the Event FMQ
TEST_P(SensorsHidlTest, InjectSensorEventData) {
  std::vector<SensorInfoType> sensors = getInjectEventSensors();
  if (sensors.size() == 0) {
    return;
  }

  ASSERT_EQ(Result::OK, getSensors()->setOperationMode(OperationMode::DATA_INJECTION));

  EventCallback callback;
  getEnvironment()->registerCallback(&callback);

  // AdditionalInfo event should not be sent to Event FMQ
  EventType additionalInfoEvent;
  additionalInfoEvent.sensorType = SensorTypeVersion::ADDITIONAL_INFO;
  additionalInfoEvent.timestamp = android::elapsedRealtimeNano();

  EventType injectedEvent;
  injectedEvent.timestamp = android::elapsedRealtimeNano();
  Vec3 data = {1, 2, 3, SensorStatus::ACCURACY_HIGH};
  injectedEvent.u.vec3 = data;

  for (const auto& s : sensors) {
    additionalInfoEvent.sensorHandle = s.sensorHandle;
    EXPECT_EQ(Result::OK, getSensors()->injectSensorData(additionalInfoEvent));

    injectedEvent.sensorType = s.type;
    injectedEvent.sensorHandle = s.sensorHandle;
    EXPECT_EQ(Result::OK, getSensors()->injectSensorData(injectedEvent));
  }

  // Wait for events to be written back to the Event FMQ
  callback.waitForEvents(sensors, milliseconds(1000) /* timeout */);
  getEnvironment()->unregisterCallback();

  for (const auto& s : sensors) {
    auto events = callback.getEvents(s.sensorHandle);
    auto lastEvent = events.back();
    SCOPED_TRACE(::testing::Message() << " handle=0x" << std::hex << std::setw(8) << std::setfill('0') << s.sensorHandle
                                      << std::dec << " type=" << static_cast<int>(s.type) << " name=" << s.name);

    // Verify that only a single event has been received
    ASSERT_EQ(events.size(), 1);

    // Verify that the event received matches the event injected and is not the additional
    // info event
    ASSERT_EQ(lastEvent.sensorType, s.type);
    ASSERT_EQ(lastEvent.sensorType, s.type);
    ASSERT_EQ(lastEvent.timestamp, injectedEvent.timestamp);
    ASSERT_EQ(lastEvent.u.vec3.x, injectedEvent.u.vec3.x);
    ASSERT_EQ(lastEvent.u.vec3.y, injectedEvent.u.vec3.y);
    ASSERT_EQ(lastEvent.u.vec3.z, injectedEvent.u.vec3.z);
    ASSERT_EQ(lastEvent.u.vec3.status, injectedEvent.u.vec3.status);
  }

  ASSERT_EQ(Result::OK, getSensors()->setOperationMode(OperationMode::NORMAL));
}

void SensorsHidlTest::activateAllSensors(bool enable) {
  for (const SensorInfoType& sensorInfo : getSensorsList()) {
    if (isValidType(sensorInfo.type)) {
      batch(sensorInfo.sensorHandle, sensorInfo.minDelay * 1000LL, 0 /* maxReportLatencyNs */);
      activate(sensorInfo.sensorHandle, enable);
    }
  }
}

// Test that if initialize is called twice, then the HAL writes events to the FMQs from the second
// call to the function.
TEST_P(SensorsHidlTest, CallInitializeTwice) {
  // Create a helper class so that a second environment is able to be instantiated
  class SensorsHidlEnvironmentTest : public SensorsHidlEnvironmentV2_X {
  public:
    SensorsHidlEnvironmentTest(const std::string& service_name) : SensorsHidlEnvironmentV2_X(service_name) {}
  };

  if (getSensorsList().size() == 0) {
    // No sensors
    return;
  }

  constexpr useconds_t kCollectionTimeoutUs = 5000 * 1000;  // 5s
  constexpr int32_t kNumEvents = 1;

  // Create a new environment that calls initialize()
  std::unique_ptr<SensorsHidlEnvironmentTest> newEnv = std::make_unique<SensorsHidlEnvironmentTest>(GetParam());
  newEnv->SetUp();
  if (HasFatalFailure()) {
    return;  // Exit early if setting up the new environment failed
  }

  activateAllSensors(true);
  // Verify that the old environment does not receive any events
  EXPECT_EQ(getEnvironment()->collectEvents(kCollectionTimeoutUs, kNumEvents).size(), 0);
  // Verify that the new event queue receives sensor events
  EXPECT_GE(newEnv.get()->collectEvents(kCollectionTimeoutUs, kNumEvents).size(), kNumEvents);
  activateAllSensors(false);

  // Cleanup the test environment
  newEnv->TearDown();

  // Restore the test environment for future tests
  getEnvironment()->TearDown();
  getEnvironment()->SetUp();
  if (HasFatalFailure()) {
    return;  // Exit early if resetting the environment failed
  }

  // Ensure that the original environment is receiving events
  activateAllSensors(true);
  EXPECT_GE(getEnvironment()->collectEvents(kCollectionTimeoutUs, kNumEvents).size(), kNumEvents);
  activateAllSensors(false);
}

TEST_P(SensorsHidlTest, CleanupConnectionsOnInitialize) {
  if (getSensorsList().size() == 0) {
    // No sensors
    return;
  }

  activateAllSensors(true);

  // Verify that events are received
  constexpr useconds_t kCollectionTimeoutUs = 5000 * 1000;  // 5s
  constexpr int32_t kNumEvents = 1;
  ASSERT_GE(getEnvironment()->collectEvents(kCollectionTimeoutUs, kNumEvents).size(), kNumEvents);

  // Clear the active sensor handles so they are not disabled during TearDown
  auto handles = mSensorHandles;
  mSensorHandles.clear();
  getEnvironment()->TearDown();
  getEnvironment()->SetUp();
  if (HasFatalFailure()) {
    return;  // Exit early if resetting the environment failed
  }

  // Verify no events are received until sensors are re-activated
  ASSERT_EQ(getEnvironment()->collectEvents(kCollectionTimeoutUs, kNumEvents).size(), 0);
  activateAllSensors(true);
  ASSERT_GE(getEnvironment()->collectEvents(kCollectionTimeoutUs, kNumEvents).size(), kNumEvents);

  // Disable sensors
  activateAllSensors(false);

  // Restore active sensors prior to clearing the environment
  mSensorHandles = handles;
}

void SensorsHidlTest::runSingleFlushTest(const std::vector<SensorInfoType>& sensors, bool activateSensor,
                                         int32_t expectedFlushCount, Result expectedResponse) {
  runFlushTest(sensors, activateSensor, 1 /* flushCalls */, expectedFlushCount, expectedResponse);
}

void SensorsHidlTest::runFlushTest(const std::vector<SensorInfoType>& sensors, bool activateSensor, int32_t flushCalls,
                                   int32_t expectedFlushCount, Result expectedResponse) {
  EventCallback callback;
  getEnvironment()->registerCallback(&callback);

  for (const SensorInfoType& sensor : sensors) {
    // Configure and activate the sensor
    batch(sensor.sensorHandle, sensor.maxDelay * 1000LL, 0 /* maxReportLatencyNs */);
    activate(sensor.sensorHandle, activateSensor);

    // Flush the sensor
    for (int32_t i = 0; i < flushCalls; i++) {
      SCOPED_TRACE(::testing::Message() << "Flush " << i << "/" << flushCalls << ": "
                                        << " handle=0x" << std::hex << std::setw(8) << std::setfill('0')
                                        << sensor.sensorHandle << std::dec << " type=" << static_cast<int>(sensor.type)
                                        << " name=" << sensor.name);

      Result flushResult = flush(sensor.sensorHandle);
      EXPECT_EQ(flushResult, expectedResponse);
    }
  }

  // Wait up to one second for the flush events
  callback.waitForFlushEvents(sensors, flushCalls, milliseconds(1000) /* timeout */);

  // Deactivate all sensors after waiting for flush events so pending flush events are not
  // abandoned by the HAL.
  for (const SensorInfoType& sensor : sensors) {
    activate(sensor.sensorHandle, false);
  }
  getEnvironment()->unregisterCallback();

  // Check that the correct number of flushes are present for each sensor
  for (const SensorInfoType& sensor : sensors) {
    SCOPED_TRACE(::testing::Message() << " handle=0x" << std::hex << std::setw(8) << std::setfill('0')
                                      << sensor.sensorHandle << std::dec << " type=" << static_cast<int>(sensor.type)
                                      << " name=" << sensor.name);
    ASSERT_EQ(callback.getFlushCount(sensor.sensorHandle), expectedFlushCount);
  }
}

TEST_P(SensorsHidlTest, FlushSensor) {
  // Find a sensor that is not a one-shot sensor
  std::vector<SensorInfoType> sensors = getNonOneShotSensors();
  if (sensors.size() == 0) {
    return;
  }

  constexpr int32_t kFlushes = 5;
  runSingleFlushTest(sensors, true /* activateSensor */, 1 /* expectedFlushCount */, Result::OK);
  runFlushTest(sensors, true /* activateSensor */, kFlushes, kFlushes, Result::OK);
}

TEST_P(SensorsHidlTest, FlushOneShotSensor) {
  // Find a sensor that is a one-shot sensor
  std::vector<SensorInfoType> sensors = getOneShotSensors();
  if (sensors.size() == 0) {
    return;
  }

  runSingleFlushTest(sensors, true /* activateSensor */, 0 /* expectedFlushCount */, Result::BAD_VALUE);
}

TEST_P(SensorsHidlTest, FlushInactiveSensor) {
  // Attempt to find a non-one shot sensor, then a one-shot sensor if necessary
  std::vector<SensorInfoType> sensors = getNonOneShotSensors();
  if (sensors.size() == 0) {
    sensors = getOneShotSensors();
    if (sensors.size() == 0) {
      return;
    }
  }

  runSingleFlushTest(sensors, false /* activateSensor */, 0 /* expectedFlushCount */, Result::BAD_VALUE);
}

TEST_P(SensorsHidlTest, Batch) {
  if (getSensorsList().size() == 0) {
    return;
  }

  activateAllSensors(false /* enable */);
  for (const SensorInfoType& sensor : getSensorsList()) {
    SCOPED_TRACE(::testing::Message() << " handle=0x" << std::hex << std::setw(8) << std::setfill('0')
                                      << sensor.sensorHandle << std::dec << " type=" << static_cast<int>(sensor.type)
                                      << " name=" << sensor.name);

    // Call batch on inactive sensor
    // One shot sensors have minDelay set to -1 which is an invalid
    // parameter. Use 0 instead to avoid errors.
    int64_t samplingPeriodNs =
      extractReportMode(sensor.flags) == SensorFlagBits::ONE_SHOT_MODE ? 0 : sensor.minDelay * 1000LL;
    ASSERT_EQ(batch(sensor.sensorHandle, samplingPeriodNs, 0 /* maxReportLatencyNs */), Result::OK);

    // Activate the sensor
    activate(sensor.sensorHandle, true /* enabled */);

    // Call batch on an active sensor
    ASSERT_EQ(batch(sensor.sensorHandle, sensor.maxDelay * 1000LL, 0 /* maxReportLatencyNs */), Result::OK);
  }
  activateAllSensors(false /* enable */);

  // Call batch on an invalid sensor
  SensorInfoType sensor = getSensorsList().front();
  sensor.sensorHandle = getInvalidSensorHandle();
  ASSERT_EQ(batch(sensor.sensorHandle, sensor.minDelay * 1000LL, 0 /* maxReportLatencyNs */), Result::BAD_VALUE);
}

TEST_P(SensorsHidlTest, Activate) {
  if (getSensorsList().size() == 0) {
    return;
  }

  // Verify that sensor events are generated when activate is called
  for (const SensorInfoType& sensor : getSensorsList()) {
    SCOPED_TRACE(::testing::Message() << " handle=0x" << std::hex << std::setw(8) << std::setfill('0')
                                      << sensor.sensorHandle << std::dec << " type=" << static_cast<int>(sensor.type)
                                      << " name=" << sensor.name);

    batch(sensor.sensorHandle, sensor.minDelay * 1000LL, 0 /* maxReportLatencyNs */);
    ASSERT_EQ(activate(sensor.sensorHandle, true), Result::OK);

    // Call activate on a sensor that is already activated
    ASSERT_EQ(activate(sensor.sensorHandle, true), Result::OK);

    // Deactivate the sensor
    ASSERT_EQ(activate(sensor.sensorHandle, false), Result::OK);

    // Call deactivate on a sensor that is already deactivated
    ASSERT_EQ(activate(sensor.sensorHandle, false), Result::OK);
  }

  // Attempt to activate an invalid sensor
  int32_t invalidHandle = getInvalidSensorHandle();
  ASSERT_EQ(activate(invalidHandle, true), Result::BAD_VALUE);
  ASSERT_EQ(activate(invalidHandle, false), Result::BAD_VALUE);
}

// Test if sensor name, vendor are as expected
TEST_P(SensorsHidlTest, ConfigCheck) {
  const std::regex smiPattern("SMI[0-9]+ BOSCH .* Sensor");
  getSensors()->getSensorsList([&](const auto& list) {
    for (const auto& sensor : list) {
      EXPECT_TRUE(std::regex_match(sensor.name.c_str(), smiPattern));
      EXPECT_STREQ(sensor.vendor.c_str(), "Robert Bosch GmbH");
    }
  });
}

TEST_P(SensorsHidlTest, NoStaleEvents) {
  constexpr milliseconds kFiveHundredMs(500);
  constexpr milliseconds kOneSecond(1000);

  // Register the callback to receive sensor events
  EventCallback callback;
  getEnvironment()->registerCallback(&callback);

  // This test is not valid for one-shot, on-change or special-report-mode sensors
  const std::vector<SensorInfoType> sensors = getNonOneShotAndNonOnChangeAndNonSpecialSensors();
  milliseconds maxMinDelay(0);
  for (const SensorInfoType& sensor : sensors) {
    milliseconds minDelay = duration_cast<milliseconds>(microseconds(sensor.minDelay));
    maxMinDelay = milliseconds(std::max(maxMinDelay.count(), minDelay.count()));
  }

  // Activate the sensors so that they start generating events
  activateAllSensors(true);

  // According to the CDD, the first sample must be generated within 400ms + 2 * sample_time
  // and the maximum reporting latency is 100ms + 2 * sample_time. Wait a sufficient amount
  // of time to guarantee that a sample has arrived.
  callback.waitForEvents(sensors, kFiveHundredMs + (5 * maxMinDelay));
  activateAllSensors(false);

  // Save the last received event for each sensor
  std::map<int32_t, int64_t> lastEventTimestampMap;
  for (const SensorInfoType& sensor : sensors) {
    SCOPED_TRACE(::testing::Message() << " handle=0x" << std::hex << std::setw(8) << std::setfill('0')
                                      << sensor.sensorHandle << std::dec << " type=" << static_cast<int>(sensor.type)
                                      << " name=" << sensor.name);

    if (callback.getEvents(sensor.sensorHandle).size() >= 1) {
      lastEventTimestampMap[sensor.sensorHandle] = callback.getEvents(sensor.sensorHandle).back().timestamp;
    }
  }

  // Allow some time to pass, reset the callback, then reactivate the sensors
  usleep(duration_cast<microseconds>(kOneSecond + (5 * maxMinDelay)).count());
  callback.reset();
  activateAllSensors(true);
  callback.waitForEvents(sensors, kFiveHundredMs + (5 * maxMinDelay));
  activateAllSensors(false);

  getEnvironment()->unregisterCallback();

  for (const SensorInfoType& sensor : sensors) {
    SCOPED_TRACE(::testing::Message() << " handle=0x" << std::hex << std::setw(8) << std::setfill('0')
                                      << sensor.sensorHandle << std::dec << " type=" << static_cast<int>(sensor.type)
                                      << " name=" << sensor.name);

    // Skip sensors that did not previously report an event
    if (lastEventTimestampMap.find(sensor.sensorHandle) == lastEventTimestampMap.end()) {
      continue;
    }

    // Ensure that the first event received is not stale by ensuring that its timestamp is
    // sufficiently different from the previous event
    const EventType newEvent = callback.getEvents(sensor.sensorHandle).front();
    milliseconds delta =
      duration_cast<milliseconds>(nanoseconds(newEvent.timestamp - lastEventTimestampMap[sensor.sensorHandle]));
    milliseconds sensorMinDelay = duration_cast<milliseconds>(microseconds(sensor.minDelay));
    ASSERT_GE(delta, kFiveHundredMs + (3 * sensorMinDelay));
  }
}

void SensorsHidlTest::checkRateLevel(const SensorInfoType& sensor, int32_t directChannelHandle, RateLevel rateLevel) {
  configDirectReport(sensor.sensorHandle, directChannelHandle, rateLevel, [&](Result result, int32_t reportToken) {
    SCOPED_TRACE(::testing::Message() << " handle=0x" << std::hex << std::setw(8) << std::setfill('0')
                                      << sensor.sensorHandle << std::dec << " type=" << static_cast<int>(sensor.type)
                                      << " name=" << sensor.name);

    if (isDirectReportRateSupported(sensor, rateLevel)) {
      ASSERT_EQ(result, Result::OK);
      if (rateLevel != RateLevel::STOP) {
        ASSERT_GT(reportToken, 0);
      }
    } else {
      ASSERT_EQ(result, Result::BAD_VALUE);
    }
  });
}

void SensorsHidlTest::queryDirectChannelSupport(SharedMemType memType, bool* supportsSharedMemType,
                                                bool* supportsAnyDirectChannel) {
  *supportsSharedMemType = false;
  *supportsAnyDirectChannel = false;
  for (const SensorInfoType& curSensor : getSensorsList()) {
    if (isDirectChannelTypeSupported(curSensor, memType)) {
      *supportsSharedMemType = true;
    }
    if (isDirectChannelTypeSupported(curSensor, SharedMemType::ASHMEM) ||
        isDirectChannelTypeSupported(curSensor, SharedMemType::GRALLOC)) {
      *supportsAnyDirectChannel = true;
    }

    if (*supportsSharedMemType && *supportsAnyDirectChannel) {
      break;
    }
  }
}

void SensorsHidlTest::verifyRegisterDirectChannel(
  std::shared_ptr<SensorsTestSharedMemory<SensorTypeVersion, EventType>> mem, int32_t* directChannelHandle,
  bool supportsSharedMemType, bool supportsAnyDirectChannel) {
  char* buffer = mem->getBuffer();
  size_t size = mem->getSize();

  if (supportsSharedMemType) {
    memset(buffer, 0xff, size);
  }

  registerDirectChannel(mem->getSharedMemInfo(), [&](Result result, int32_t channelHandle) {
    if (supportsSharedMemType) {
      ASSERT_EQ(result, Result::OK);
      ASSERT_GT(channelHandle, 0);

      // Verify that the memory has been zeroed
      for (size_t i = 0; i < mem->getSize(); i++) {
        ASSERT_EQ(buffer[i], 0x00);
      }
    } else {
      Result expectedResult = supportsAnyDirectChannel ? Result::BAD_VALUE : Result::INVALID_OPERATION;
      ASSERT_EQ(result, expectedResult);
      ASSERT_EQ(channelHandle, -1);
    }
    *directChannelHandle = channelHandle;
  });
}

void SensorsHidlTest::verifyConfigure(const SensorInfoType& sensor, SharedMemType memType, int32_t directChannelHandle,
                                      bool supportsAnyDirectChannel) {
  SCOPED_TRACE(::testing::Message() << " handle=0x" << std::hex << std::setw(8) << std::setfill('0')
                                    << sensor.sensorHandle << std::dec << " type=" << static_cast<int>(sensor.type)
                                    << " name=" << sensor.name);

  if (isDirectChannelTypeSupported(sensor, memType)) {
    // Verify that each rate level is properly supported
    checkRateLevel(sensor, directChannelHandle, RateLevel::NORMAL);
    checkRateLevel(sensor, directChannelHandle, RateLevel::FAST);
    checkRateLevel(sensor, directChannelHandle, RateLevel::VERY_FAST);
    checkRateLevel(sensor, directChannelHandle, RateLevel::STOP);

    // Verify that a sensor handle of -1 is only acceptable when using RateLevel::STOP
    configDirectReport(-1 /* sensorHandle */, directChannelHandle, RateLevel::NORMAL,
                       [](Result result, int32_t /* reportToken */) { ASSERT_EQ(result, Result::BAD_VALUE); });
    configDirectReport(-1 /* sensorHandle */, directChannelHandle, RateLevel::STOP,
                       [](Result result, int32_t /* reportToken */) { ASSERT_EQ(result, Result::OK); });
  } else {
    // directChannelHandle will be -1 here, HAL should either reject it as a bad value if there
    // is some level of direct channel report, otherwise return INVALID_OPERATION if direct
    // channel is not supported at all
    Result expectedResult = supportsAnyDirectChannel ? Result::BAD_VALUE : Result::INVALID_OPERATION;
    configDirectReport(
      sensor.sensorHandle, directChannelHandle, RateLevel::NORMAL,
      [expectedResult](Result result, int32_t /* reportToken */) { ASSERT_EQ(result, expectedResult); });
  }
}

void SensorsHidlTest::verifyUnregisterDirectChannel(int32_t directChannelHandle, bool supportsAnyDirectChannel) {
  Result expectedResult = supportsAnyDirectChannel ? Result::OK : Result::INVALID_OPERATION;
  ASSERT_EQ(unregisterDirectChannel(directChannelHandle), expectedResult);
}

void SensorsHidlTest::verifyDirectChannel(SharedMemType memType) {
  constexpr size_t kNumEvents = 1;
  constexpr size_t kMemSize = kNumEvents * kEventSize;

  bool supportsSharedMemType;
  bool supportsAnyDirectChannel;
  queryDirectChannelSupport(memType, &supportsSharedMemType, &supportsAnyDirectChannel);

  if (supportsSharedMemType) {
    std::shared_ptr<SensorsTestSharedMemory<SensorTypeVersion, EventType>> mem(
      SensorsTestSharedMemory<SensorTypeVersion, EventType>::create(memType, kMemSize));
    ASSERT_NE(mem, nullptr);

    for (const SensorInfoType& sensor : getSensorsList()) {
      int32_t directChannelHandle = 0;
      verifyRegisterDirectChannel(mem, &directChannelHandle, supportsSharedMemType, supportsAnyDirectChannel);
      verifyConfigure(sensor, memType, directChannelHandle, supportsAnyDirectChannel);
      verifyUnregisterDirectChannel(directChannelHandle, supportsAnyDirectChannel);
    }
  }
}

TEST_P(SensorsHidlTest, DirectChannelAshmem) { verifyDirectChannel(SharedMemType::ASHMEM); }

TEST_P(SensorsHidlTest, DirectChannelGralloc) { verifyDirectChannel(SharedMemType::GRALLOC); }

bool SensorsHidlTest::getDirectChannelSensor(SensorInfoType* sensor, SharedMemType* memType, RateLevel* rate) {
  bool found = false;
  for (const SensorInfoType& curSensor : getSensorsList()) {
    if (isDirectChannelTypeSupported(curSensor, SharedMemType::ASHMEM)) {
      *memType = SharedMemType::ASHMEM;
      *sensor = curSensor;
      found = true;
      break;
    } else if (isDirectChannelTypeSupported(curSensor, SharedMemType::GRALLOC)) {
      *memType = SharedMemType::GRALLOC;
      *sensor = curSensor;
      found = true;
      break;
    }
  }

  if (found) {
    // Find a supported rate level
    constexpr int kNumRateLevels = 3;
    RateLevel rates[kNumRateLevels] = {RateLevel::NORMAL, RateLevel::FAST, RateLevel::VERY_FAST};
    *rate = RateLevel::STOP;
    for (int i = 0; i < kNumRateLevels; i++) {
      if (isDirectReportRateSupported(*sensor, rates[i])) {
        *rate = rates[i];
      }
    }

    // At least one rate level must be supported
    EXPECT_NE(*rate, RateLevel::STOP);
  }
  return found;
}

constexpr bool inRange(float value, float lowerLimit, float upperLimit) {
  return ((value >= lowerLimit) && (value <= upperLimit));
}

void SensorsHidlTest::checkVec3Sensor(SensorTypeVersion type, const Vec3& lowerLimit, const Vec3& upperLimit) {
  std::vector<EventType> events;
  constexpr useconds_t kCollectionTimeoutUs = 5000 * 1000;  // 5s
  constexpr int32_t kNumEvents = 20;

  SensorInfoType sensor = defaultSensorByType(type);
  if (!isValidType(sensor.type)) {
    // no default sensor of this type
    return;
  }
  const int32_t handle = sensor.sensorHandle;

  ASSERT_EQ(activate(handle, 1), Result::OK);
  if (type == SensorTypeVersion::GRAVITY || type == SensorTypeVersion::LINEAR_ACCELERATION) {
    // These sensors are required to be calibrated
    ASSERT_EQ(batch(handle, sensor.minDelay * 1000LL, 0 /* maxReportLatencyNs */), Result::OK);
    usleep(sensor.minDelay * 50LL);
  }
  events = getEnvironment()->collectEvents(kCollectionTimeoutUs, kNumEvents);
  ASSERT_EQ(activate(handle, 0), Result::OK);
  ASSERT_GE(events.size(), kNumEvents);

  float xSum = 0.0f;
  float ySum = 0.0f;
  float zSum = 0.0f;
  for (const auto& ev : events) {
    xSum += ev.u.vec3.x;
    ySum += ev.u.vec3.y;
    zSum += ev.u.vec3.z;
  }
  const float xAvg = xSum / events.size();
  const float yAvg = ySum / events.size();
  const float zAvg = zSum / events.size();

  ALOGD("Average values, x: %f, y: %f, z: %f", xAvg, yAvg, zAvg);
  EXPECT_TRUE(inRange(xAvg, lowerLimit.x, upperLimit.x));
  EXPECT_TRUE(inRange(yAvg, lowerLimit.y, upperLimit.y));
  EXPECT_TRUE(inRange(zAvg, lowerLimit.z, upperLimit.z));
}

// Test if accelerometer sensor vector is in range
TEST_P(SensorsHidlTest, AccelerometerCheckSensorVector) {
  const Vec3 lowerLimit = {-1.0, -1.0, GRAVITY_EARTH - 1.0, SensorStatus::ACCURACY_HIGH};
  const Vec3 upperLimit = {1.0, 1.0, GRAVITY_EARTH + 1.0, SensorStatus::ACCURACY_HIGH};
  checkVec3Sensor(SensorTypeVersion::ACCELEROMETER, lowerLimit, upperLimit);
}

// Test if gyroscope sensor vector is in range
TEST_P(SensorsHidlTest, GyroscopeCheckSensorVector) {
  const Vec3 lowerLimit = {-1.0, -1.0, -1.0, SensorStatus::ACCURACY_HIGH};
  const Vec3 upperLimit = {1.0, 1.0, 1.0, SensorStatus::ACCURACY_HIGH};
  checkVec3Sensor(SensorTypeVersion::GYROSCOPE, lowerLimit, upperLimit);
}

// Test if gravity sensor vector is in range
TEST_P(SensorsHidlTest, GravityCheckSensorVector) {
  const Vec3 lowerLimit = {-1.0, -1.0, GRAVITY_EARTH - 1.0, SensorStatus::ACCURACY_HIGH};
  const Vec3 upperLimit = {1.0, 1.0, GRAVITY_EARTH + 1.0, SensorStatus::ACCURACY_HIGH};
  checkVec3Sensor(SensorTypeVersion::GRAVITY, lowerLimit, upperLimit);
}

// Test if linear acceleration sensor vector is in range
TEST_P(SensorsHidlTest, LinearAccelerationCheckSensorVector) {
  const Vec3 lowerLimit = {-1.0, -1.0, -1.0, SensorStatus::ACCURACY_HIGH};
  const Vec3 upperLimit = {1.0, 1.0, 1.0, SensorStatus::ACCURACY_HIGH};
  checkVec3Sensor(SensorTypeVersion::LINEAR_ACCELERATION, lowerLimit, upperLimit);
}

// Test if sensor hal can do UI speed accelerometer streaming properly
TEST_P(SensorsHidlTest, AccelerometerStreamingOperationSlow) {
  testStreamingOperation(SensorTypeVersion::ACCELEROMETER, std::chrono::milliseconds(200), std::chrono::seconds(5),
                         mAccelNormChecker);
}

// Test if sensor hal can do normal speed accelerometer streaming properly
TEST_P(SensorsHidlTest, AccelerometerStreamingOperationNormal) {
  testStreamingOperation(SensorTypeVersion::ACCELEROMETER, std::chrono::milliseconds(20), std::chrono::seconds(5),
                         mAccelNormChecker);
}

// Test if sensor hal can do game speed accelerometer streaming properly
TEST_P(SensorsHidlTest, AccelerometerStreamingOperationFast) {
  testStreamingOperation(SensorTypeVersion::ACCELEROMETER, std::chrono::milliseconds(10), std::chrono::seconds(5),
                         mAccelNormChecker);
}

// Test if sensor hal can do UI speed gyroscope streaming properly
TEST_P(SensorsHidlTest, GyroscopeStreamingOperationSlow) {
  testStreamingOperation(SensorTypeVersion::GYROSCOPE, std::chrono::milliseconds(200), std::chrono::seconds(5),
                         mGyroNormChecker);
}

// Test if sensor hal can do normal speed gyroscope streaming properly
TEST_P(SensorsHidlTest, GyroscopeStreamingOperationNormal) {
  testStreamingOperation(SensorTypeVersion::GYROSCOPE, std::chrono::milliseconds(20), std::chrono::seconds(5),
                         mGyroNormChecker);
}

// Test if sensor hal can do game speed gyroscope streaming properly
TEST_P(SensorsHidlTest, GyroscopeStreamingOperationFast) {
  testStreamingOperation(SensorTypeVersion::GYROSCOPE, std::chrono::milliseconds(10), std::chrono::seconds(5),
                         mGyroNormChecker);
}

// Test if sensor hal can do game speed gravity streaming properly
TEST_P(SensorsHidlTest, GravityStreamingOperationFast) {
  testStreamingOperation(SensorTypeVersion::GRAVITY, std::chrono::milliseconds(10), std::chrono::seconds(5),
                         NullChecker<EventType>());
}

// Test if sensor hal can do game speed linear acceleration streaming properly
TEST_P(SensorsHidlTest, LinearAccelerationStreamingOperationFast) {
  testStreamingOperation(SensorTypeVersion::LINEAR_ACCELERATION, std::chrono::milliseconds(10), std::chrono::seconds(5),
                         NullChecker<EventType>());
}

// Test if sensor hal can do accelerometer sampling rate switch properly when sensor is active
TEST_P(SensorsHidlTest, AccelerometerSamplingPeriodHotSwitchOperation) {
  testSamplingRateHotSwitchOperation(SensorTypeVersion::ACCELEROMETER);
  testSamplingRateHotSwitchOperation(SensorTypeVersion::ACCELEROMETER, false /*fastToSlow*/);
}

// Test if sensor hal can do gyroscope sampling rate switch properly when sensor is active
TEST_P(SensorsHidlTest, GyroscopeSamplingPeriodHotSwitchOperation) {
  testSamplingRateHotSwitchOperation(SensorTypeVersion::GYROSCOPE);
  testSamplingRateHotSwitchOperation(SensorTypeVersion::GYROSCOPE, false /*fastToSlow*/);
}

// Test if sensor hal can do gravity sampling rate switch properly when sensor is active
TEST_P(SensorsHidlTest, GravitySamplingPeriodHotSwitchOperation) {
  testSamplingRateHotSwitchOperation(SensorTypeVersion::GRAVITY);
  testSamplingRateHotSwitchOperation(SensorTypeVersion::GRAVITY, false /*fastToSlow*/);
}

// Test if sensor hal can do gravity sampling rate switch properly when sensor is active
TEST_P(SensorsHidlTest, LinearAccelerationSamplingPeriodHotSwitchOperation) {
  testSamplingRateHotSwitchOperation(SensorTypeVersion::LINEAR_ACCELERATION);
  testSamplingRateHotSwitchOperation(SensorTypeVersion::LINEAR_ACCELERATION, false /*fastToSlow*/);
}

// Test if sensor hal can do accelerometer batching properly
TEST_P(SensorsHidlTest, AccelerometerBatchingOperation) { testBatchingOperation(SensorTypeVersion::ACCELEROMETER); }

// Test if sensor hal can do gyroscope batching properly
TEST_P(SensorsHidlTest, GyroscopeBatchingOperation) { testBatchingOperation(SensorTypeVersion::GYROSCOPE); }

// Test if sensor hal can do gravity batching properly
TEST_P(SensorsHidlTest, GravityBatchingOperation) { testBatchingOperation(SensorTypeVersion::GRAVITY); }

// Test if sensor hal can do linear acceleration batching properly
TEST_P(SensorsHidlTest, LinearAccelerationBatchingOperation) {
  testBatchingOperation(SensorTypeVersion::LINEAR_ACCELERATION);
}

// Test sensor event direct report with ashmem for accel sensor at normal rate
TEST_P(SensorsHidlTest, AccelerometerAshmemDirectReportOperationNormal) {
  testDirectReportOperation(SensorTypeVersion::ACCELEROMETER, SharedMemType::ASHMEM, RateLevel::NORMAL,
                            mAccelNormChecker);
}

// Test sensor event direct report with ashmem for accel sensor at fast rate
TEST_P(SensorsHidlTest, AccelerometerAshmemDirectReportOperationFast) {
  testDirectReportOperation(SensorTypeVersion::ACCELEROMETER, SharedMemType::ASHMEM, RateLevel::FAST,
                            mAccelNormChecker);
}

// Test sensor event direct report with ashmem for accel sensor at very fast rate
TEST_P(SensorsHidlTest, AccelerometerAshmemDirectReportOperationVeryFast) {
  testDirectReportOperation(SensorTypeVersion::ACCELEROMETER, SharedMemType::ASHMEM, RateLevel::VERY_FAST,
                            mAccelNormChecker);
}

// Test sensor event direct report with ashmem for gyro sensor at normal rate
TEST_P(SensorsHidlTest, GyroscopeAshmemDirectReportOperationNormal) {
  testDirectReportOperation(SensorTypeVersion::GYROSCOPE, SharedMemType::ASHMEM, RateLevel::NORMAL, mGyroNormChecker);
}

// Test sensor event direct report with ashmem for gyro sensor at fast rate
TEST_P(SensorsHidlTest, GyroscopeAshmemDirectReportOperationFast) {
  testDirectReportOperation(SensorTypeVersion::GYROSCOPE, SharedMemType::ASHMEM, RateLevel::FAST, mGyroNormChecker);
}

// Test sensor event direct report with ashmem for gyro sensor at very fast rate
TEST_P(SensorsHidlTest, GyroscopeAshmemDirectReportOperationVeryFast) {
  testDirectReportOperation(SensorTypeVersion::GYROSCOPE, SharedMemType::ASHMEM, RateLevel::VERY_FAST,
                            mGyroNormChecker);
}

// Test sensor event direct report with ashmem for gravity sensor at normal rate
TEST_P(SensorsHidlTest, GravityAshmemDirectReportOperationNormal) {
  testDirectReportOperation(SensorTypeVersion::GRAVITY, SharedMemType::ASHMEM, RateLevel::NORMAL,
                            NullChecker<EventType>());
}

// Test sensor event direct report with ashmem for gravity sensor at fast rate
TEST_P(SensorsHidlTest, GravityAshmemDirectReportOperationFast) {
  testDirectReportOperation(SensorTypeVersion::GRAVITY, SharedMemType::ASHMEM, RateLevel::FAST,
                            NullChecker<EventType>());
}

// Test sensor event direct report with ashmem for gravity sensor at very fast rate
TEST_P(SensorsHidlTest, GravityAshmemDirectReportOperationVeryFast) {
  testDirectReportOperation(SensorTypeVersion::GRAVITY, SharedMemType::ASHMEM, RateLevel::VERY_FAST,
                            NullChecker<EventType>());
}

// Test sensor event direct report with ashmem for linear acceleration sensor at normal rate
TEST_P(SensorsHidlTest, LinearAccelerationAshmemDirectReportOperationNormal) {
  testDirectReportOperation(SensorTypeVersion::LINEAR_ACCELERATION, SharedMemType::ASHMEM, RateLevel::NORMAL,
                            NullChecker<EventType>());
}

// Test sensor event direct report with ashmem for linear acceleration sensor at fast rate
TEST_P(SensorsHidlTest, LinearAccelerationAshmemDirectReportOperationFast) {
  testDirectReportOperation(SensorTypeVersion::LINEAR_ACCELERATION, SharedMemType::ASHMEM, RateLevel::FAST,
                            NullChecker<EventType>());
}

// Test sensor event direct report with ashmem for linear acceleration sensor at very fast rate
TEST_P(SensorsHidlTest, LinearAccelerationAshmemDirectReportOperationVeryFast) {
  testDirectReportOperation(SensorTypeVersion::LINEAR_ACCELERATION, SharedMemType::ASHMEM, RateLevel::VERY_FAST,
                            NullChecker<EventType>());
}

// Test sensor event direct report with gralloc for accel sensor at normal rate
TEST_P(SensorsHidlTest, AccelerometerGrallocDirectReportOperationNormal) {
  testDirectReportOperation(SensorTypeVersion::ACCELEROMETER, SharedMemType::GRALLOC, RateLevel::NORMAL,
                            mAccelNormChecker);
}

// Test sensor event direct report with gralloc for accel sensor at fast rate
TEST_P(SensorsHidlTest, AccelerometerGrallocDirectReportOperationFast) {
  testDirectReportOperation(SensorTypeVersion::ACCELEROMETER, SharedMemType::GRALLOC, RateLevel::FAST,
                            mAccelNormChecker);
}

// Test sensor event direct report with gralloc for accel sensor at very fast rate
TEST_P(SensorsHidlTest, AccelerometerGrallocDirectReportOperationVeryFast) {
  testDirectReportOperation(SensorTypeVersion::ACCELEROMETER, SharedMemType::GRALLOC, RateLevel::VERY_FAST,
                            mAccelNormChecker);
}

// Test sensor event direct report with gralloc for gyro sensor at normal rate
TEST_P(SensorsHidlTest, GyroscopeGrallocDirectReportOperationNormal) {
  testDirectReportOperation(SensorTypeVersion::GYROSCOPE, SharedMemType::GRALLOC, RateLevel::NORMAL, mGyroNormChecker);
}

// Test sensor event direct report with gralloc for gyro sensor at fast rate
TEST_P(SensorsHidlTest, GyroscopeGrallocDirectReportOperationFast) {
  testDirectReportOperation(SensorTypeVersion::GYROSCOPE, SharedMemType::GRALLOC, RateLevel::FAST, mGyroNormChecker);
}

// Test sensor event direct report with gralloc for gyro sensor at very fast rate
TEST_P(SensorsHidlTest, GyroscopeGrallocDirectReportOperationVeryFast) {
  testDirectReportOperation(SensorTypeVersion::GYROSCOPE, SharedMemType::GRALLOC, RateLevel::VERY_FAST,
                            mGyroNormChecker);
}

// Test sensor event direct report with gralloc for gravity sensor at normal rate
TEST_P(SensorsHidlTest, GravityGrallocDirectReportOperationNormal) {
  testDirectReportOperation(SensorTypeVersion::GRAVITY, SharedMemType::GRALLOC, RateLevel::NORMAL,
                            NullChecker<EventType>());
}

// Test sensor event direct report with gralloc for gravity sensor at fast rate
TEST_P(SensorsHidlTest, GravityGrallocDirectReportOperationFast) {
  testDirectReportOperation(SensorTypeVersion::GRAVITY, SharedMemType::GRALLOC, RateLevel::FAST,
                            NullChecker<EventType>());
}

// Test sensor event direct report with gralloc for gravity sensor at very fast rate
TEST_P(SensorsHidlTest, GravityGrallocDirectReportOperationVeryFast) {
  testDirectReportOperation(SensorTypeVersion::GRAVITY, SharedMemType::GRALLOC, RateLevel::VERY_FAST,
                            NullChecker<EventType>());
}

// Test sensor event direct report with gralloc for linear acceleration sensor at normal rate
TEST_P(SensorsHidlTest, LinearAccelerationGrallocDirectReportOperationNormal) {
  testDirectReportOperation(SensorTypeVersion::LINEAR_ACCELERATION, SharedMemType::GRALLOC, RateLevel::NORMAL,
                            NullChecker<EventType>());
}

// Test sensor event direct report with gralloc for linear acceleration sensor at fast rate
TEST_P(SensorsHidlTest, LinearAccelerationGrallocDirectReportOperationFast) {
  testDirectReportOperation(SensorTypeVersion::LINEAR_ACCELERATION, SharedMemType::GRALLOC, RateLevel::FAST,
                            NullChecker<EventType>());
}

// Test sensor event direct report with gralloc for linear acceleration sensor at very fast rate
TEST_P(SensorsHidlTest, LinearAccelerationGrallocDirectReportOperationVeryFast) {
  testDirectReportOperation(SensorTypeVersion::LINEAR_ACCELERATION, SharedMemType::GRALLOC, RateLevel::VERY_FAST,
                            NullChecker<EventType>());
}

TEST_P(SensorsHidlTest, ConfigureDirectChannelWithInvalidHandle) {
  SensorInfoType sensor;
  SharedMemType memType;
  RateLevel rate;
  if (!getDirectChannelSensor(&sensor, &memType, &rate)) {
    return;
  }

  // Verify that an invalid channel handle produces a BAD_VALUE result
  configDirectReport(sensor.sensorHandle, -1, rate,
                     [](Result result, int32_t /* reportToken */) { ASSERT_EQ(result, Result::BAD_VALUE); });
}

TEST_P(SensorsHidlTest, CleanupDirectConnectionOnInitialize) {
  constexpr size_t kNumEvents = 1;
  constexpr size_t kMemSize = kNumEvents * kEventSize;

  SensorInfoType sensor;
  SharedMemType memType;
  RateLevel rate;

  if (!getDirectChannelSensor(&sensor, &memType, &rate)) {
    return;
  }

  std::shared_ptr<SensorsTestSharedMemory<SensorTypeVersion, EventType>> mem(
    SensorsTestSharedMemory<SensorTypeVersion, EventType>::create(memType, kMemSize));
  ASSERT_NE(mem, nullptr);

  int32_t directChannelHandle = 0;
  registerDirectChannel(mem->getSharedMemInfo(), [&](Result result, int32_t channelHandle) {
    ASSERT_EQ(result, Result::OK);
    directChannelHandle = channelHandle;
  });

  // Configure the channel and expect success
  configDirectReport(sensor.sensorHandle, directChannelHandle, rate,
                     [](Result result, int32_t /* reportToken */) { ASSERT_EQ(result, Result::OK); });

  // Call initialize() via the environment setup to cause the HAL to re-initialize
  // Clear the active direct connections so they are not stopped during TearDown
  auto handles = mDirectChannelHandles;
  mDirectChannelHandles.clear();
  getEnvironment()->TearDown();
  getEnvironment()->SetUp();
  if (HasFatalFailure()) {
    return;  // Exit early if resetting the environment failed
  }

  // Attempt to configure the direct channel and expect it to fail
  configDirectReport(sensor.sensorHandle, directChannelHandle, rate,
                     [](Result result, int32_t /* reportToken */) { ASSERT_EQ(result, Result::BAD_VALUE); });

  // Restore original handles, though they should already be deactivated
  mDirectChannelHandles = handles;
}

TEST_P(SensorsHidlTest, SensorListDoesntContainInvalidType) {
  getSensors()->getSensorsList([&](const auto& list) {
    const size_t count = list.size();
    for (size_t i = 0; i < count; ++i) {
      const auto& s = list[i];
      EXPECT_FALSE(s.type == ::android::hardware::sensors::V2_1::SensorType::HINGE_ANGLE);
    }
  });
}

TEST_P(SensorsHidlTest, FlushNonexistentSensor) {
  SensorInfoType sensor;
  std::vector<SensorInfoType> sensors = getNonOneShotSensors();
  if (sensors.size() == 0) {
    sensors = getOneShotSensors();
    if (sensors.size() == 0) {
      return;
    }
  }
  sensor = sensors.front();
  sensor.sensorHandle = getInvalidSensorHandle();
  runSingleFlushTest(std::vector<SensorInfoType>{sensor}, false /* activateSensor */, 0 /* expectedFlushCount */,
                     Result::BAD_VALUE);
}
