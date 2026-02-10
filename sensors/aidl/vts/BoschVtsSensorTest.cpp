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
#include <aidl/Gtest.h>
#include <aidl/Vintf.h>
#include <aidl/android/hardware/sensors/BnSensors.h>
#include <aidl/android/hardware/sensors/ISensors.h>
#include <android/binder_manager.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <hardware/sensors.h>
#include <log/log.h>
#include <utils/SystemClock.h>

#include <cinttypes>
#include <condition_variable>
#include <map>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "SensorsAidlEnvironment.h"
#include "SensorsAidlTestSharedMemory.h"
#include "sensors-vts-utils/SensorsVtsEnvironmentBase.h"

using aidl::android::hardware::sensors::AdditionalInfo;
using aidl::android::hardware::sensors::Event;
using aidl::android::hardware::sensors::ISensors;
using aidl::android::hardware::sensors::SensorInfo;
using aidl::android::hardware::sensors::SensorStatus;
using aidl::android::hardware::sensors::SensorType;
using android::ProcessState;
using std::chrono::duration_cast;

constexpr size_t kEventSize = static_cast<size_t>(ISensors::DIRECT_REPORT_SENSOR_EVENT_TOTAL_LENGTH);

namespace {
void checkIsOk(ndk::ScopedAStatus status) { ASSERT_TRUE(status.isOk()); }
}  // namespace

class BoschSensorTest : public testing::TestWithParam<std::string> {
public:
  virtual void SetUp() override {
    mEnvironment = new SensorsAidlEnvironment(GetParam());
    mEnvironment->SetUp();
    ALOGD("####################### %s #######################",
          ::testing::UnitTest::GetInstance()->current_test_info()->name());

    // Ensure that we have a valid environment before performing tests
    ASSERT_NE(getSensors(), nullptr);
  }

  virtual void TearDown() override {
    for (int32_t handle : mSensorHandles) {
      activate(handle, false);
    }
    mSensorHandles.clear();

    mEnvironment->TearDown();
    delete mEnvironment;
    mEnvironment = nullptr;
  }

protected:
  inline std::shared_ptr<ISensors>& getSensors() { return mEnvironment->mSensors; }

  inline SensorsAidlEnvironment* getEnvironment() { return mEnvironment; }

  inline bool isValidType(SensorType sensorType) { return (int)sensorType > 0; }

  std::vector<SensorInfo> getSensorsList();

  ndk::ScopedAStatus activate(int32_t sensorHandle, bool enable);

  ndk::ScopedAStatus batch(int32_t sensorHandle, int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
    return getSensors()->batch(sensorHandle, samplingPeriodNs, maxReportLatencyNs);
  }

  void checkVec3Sensor(SensorType type, const Event::EventPayload::Vec3& lowerLimit,
                       const Event::EventPayload::Vec3& upperLimit);

  void checkMultiChannelVector(SensorType type, const Event::EventPayload::Vec3& lowerLimit,
                               const Event::EventPayload::Vec3& upperLimit);

  void checkDirectChVector(SensorType type, const Event::EventPayload::Vec3& lowerLimit,
                           const Event::EventPayload::Vec3& upperLimit);

  // All sensors and direct channnels used
  std::unordered_set<int32_t> mSensorHandles;

private:
  SensorsAidlEnvironment* mEnvironment;
};

std::vector<SensorInfo> BoschSensorTest::getSensorsList() {
  std::vector<SensorInfo> sensorInfoList;
  checkIsOk(getSensors()->getSensorsList(&sensorInfoList));
  return sensorInfoList;
}

ndk::ScopedAStatus BoschSensorTest::activate(int32_t sensorHandle, bool enable) {
  // If activating a sensor, add the handle in a set so that when test fails it can be turned off.
  // The handle is not removed when it is deactivating on purpose so that it is not necessary to
  // check the return value of deactivation. Deactivating a sensor more than once does not have
  // negative effect.
  if (enable) {
    mSensorHandles.insert(sensorHandle);
  }
  return getSensors()->activate(sensorHandle, enable);
}

void BoschSensorTest::checkVec3Sensor(SensorType type, const Event::EventPayload::Vec3& lowerLimit,
                                      const Event::EventPayload::Vec3& upperLimit) {
  std::vector<Event> events;
  constexpr useconds_t kCollectionTimeoutUs = 50000 * 1000;  // 50s
  int32_t kNumEvents = 20;
  SensorInfo sensorInfo{};

  for (const SensorInfo& sensor : getSensorsList()) {
    if (sensor.type == type) {
      sensorInfo = sensor;
      break;
    }
  }
  if (!isValidType(sensorInfo.type)) {
    // no default sensor of this type
    return;
  }

  checkIsOk(activate(sensorInfo.sensorHandle, 0));
  usleep(1000LL);
  checkIsOk(activate(sensorInfo.sensorHandle, 1));

  if (type == SensorType::GRAVITY || type == SensorType::LINEAR_ACCELERATION) {
    // These sensors are required to be calibrated
    kNumEvents = 200;
    checkIsOk(batch(sensorInfo.sensorHandle, sensorInfo.minDelayUs * 1000LL, 0 /* maxReportLatencyNs */));
    usleep(sensorInfo.minDelayUs * 50LL);
  }
  events = getEnvironment()->collectEvents(kCollectionTimeoutUs, kNumEvents);
  checkIsOk(activate(sensorInfo.sensorHandle, 0));
  ASSERT_GE(events.size(), kNumEvents);

  float xSum = 0.0f;
  float ySum = 0.0f;
  float zSum = 0.0f;
  int validEventCount = 0;

  for (const auto& ev : events) {
    if (ev.payload.getTag() == Event::EventPayload::Tag::vec3) {
      const auto& vec3 = ev.payload.get<Event::EventPayload::Tag::vec3>();
      xSum += vec3.x;
      ySum += vec3.y;
      zSum += vec3.z;
      validEventCount++;
    }
  }
  ASSERT_GT(validEventCount, 0) << "No valid vec3 events collected.";

  const float xAvg = xSum / validEventCount;
  const float yAvg = ySum / validEventCount;
  const float zAvg = zSum / validEventCount;

  ALOGD("Average values, x: %f, y: %f, z: %f", xAvg, yAvg, zAvg);
  EXPECT_GT(xAvg, lowerLimit.x);
  EXPECT_GT(yAvg, lowerLimit.y);
  EXPECT_GT(zAvg, lowerLimit.z);
  EXPECT_LT(xAvg, upperLimit.x);
  EXPECT_LT(yAvg, upperLimit.y);
  EXPECT_LT(zAvg, upperLimit.z);
}

void BoschSensorTest::checkMultiChannelVector(SensorType type, const Event::EventPayload::Vec3& lowerLimit,
                                              const Event::EventPayload::Vec3& upperLimit) {
  std::vector<Event> events;
  useconds_t kCollectionTimeoutUs = 50000 * 1000;  // 50s
  constexpr size_t kNumEvents = 100;
  constexpr size_t kMemSize = 2 * kNumEvents * kEventSize;

  ISensors::SharedMemInfo::SharedMemType memType = ISensors::SharedMemInfo::SharedMemType::ASHMEM;
  SensorInfo sensorInfo{};
  int32_t channelHandle1, channelHandle2;
  int32_t reportToken1 = 0, reportToken2 = 0;
  constexpr useconds_t SLEEP_GRANULARITY = 1000 * 1000;  // 1s

  // Find the sensor of the specified type
  for (const SensorInfo& sensor : getSensorsList()) {
    if (sensor.type == type) {
      sensorInfo = sensor;
      break;
    }
  }
  if (!isValidType(sensorInfo.type)) {
    return;
  }

  // Allocate shared memory for two channels
  std::shared_ptr<SensorsAidlTestSharedMemory<SensorType, Event>> mem1(
    SensorsAidlTestSharedMemory<SensorType, Event>::create(memType, kMemSize));
  std::shared_ptr<SensorsAidlTestSharedMemory<SensorType, Event>> mem2(
    SensorsAidlTestSharedMemory<SensorType, Event>::create(memType, kMemSize));
  ASSERT_NE(mem1, nullptr);
  ASSERT_NE(mem2, nullptr);

  char* buffer1 = mem1->getBuffer();
  char* buffer2 = mem2->getBuffer();
  size_t size1 = mem1->getSize();
  size_t size2 = mem2->getSize();

  memset(buffer1, 0xff, size1);
  memset(buffer2, 0xff, size2);

  // Register direct channels
  ::ndk::ScopedAStatus status = getSensors()->registerDirectChannel(mem1->getSharedMemInfo(), &channelHandle1);
  ASSERT_TRUE(status.isOk());
  ASSERT_GT(channelHandle1, 0);

  status = getSensors()->registerDirectChannel(mem2->getSharedMemInfo(), &channelHandle2);
  ASSERT_TRUE(status.isOk());
  ASSERT_GT(channelHandle2, 0);

  for (size_t i = 0; i < mem1->getSize(); i++) {
    ASSERT_EQ(buffer1[i], 0x00);
  }
  for (size_t i = 0; i < mem2->getSize(); i++) {
    ASSERT_EQ(buffer2[i], 0x00);
  }

  status = getSensors()->configDirectReport(sensorInfo.sensorHandle, channelHandle1, ISensors::RateLevel::NORMAL,
                                            &reportToken1);
  ASSERT_TRUE(status.isOk());
  status = getSensors()->configDirectReport(sensorInfo.sensorHandle, channelHandle2, ISensors::RateLevel::NORMAL,
                                            &reportToken2);
  ASSERT_TRUE(status.isOk());

  while (kCollectionTimeoutUs > 0) {
    useconds_t duration = std::min(SLEEP_GRANULARITY, kCollectionTimeoutUs);
    usleep(duration);
    kCollectionTimeoutUs -= duration;

    auto events1 = mem1->parseEvents(-1, 0);
    auto events2 = mem2->parseEvents(-1, 0);

    if ((events1.size() >= kNumEvents) && (events2.size() >= kNumEvents)) {
      events.insert(events.end(), events1.begin(), events1.end());
      events.insert(events.end(), events2.begin(), events2.end());
      break;
    }
  }

  // Stop direct reports and unregister channels
  status =
    getSensors()->configDirectReport(sensorInfo.sensorHandle, channelHandle1, ISensors::RateLevel::STOP, &reportToken1);
  ASSERT_TRUE(status.isOk());
  status =
    getSensors()->configDirectReport(sensorInfo.sensorHandle, channelHandle2, ISensors::RateLevel::STOP, &reportToken2);
  ASSERT_TRUE(status.isOk());

  status = getSensors()->unregisterDirectChannel(channelHandle1);
  ASSERT_TRUE(status.isOk());
  status = getSensors()->unregisterDirectChannel(channelHandle2);
  ASSERT_TRUE(status.isOk());

  // Validate collected events
  ASSERT_GE(events.size(), kNumEvents);

  float xSum = 0.0f;
  float ySum = 0.0f;
  float zSum = 0.0f;
  int validEventCount = 0;

  for (const auto& ev : events) {
    if (ev.payload.getTag() == Event::EventPayload::Tag::vec3) {
      const auto& vec3 = ev.payload.get<Event::EventPayload::Tag::vec3>();
      xSum += vec3.x;
      ySum += vec3.y;
      zSum += vec3.z;
      validEventCount++;
    }
  }
  ASSERT_GT(validEventCount, 0) << "No valid vec3 events collected.";

  const float xAvg = xSum / validEventCount;
  const float yAvg = ySum / validEventCount;
  const float zAvg = zSum / validEventCount;

  ALOGD("Average values, x: %f, y: %f, z: %f", xAvg, yAvg, zAvg);
  EXPECT_GT(xAvg, lowerLimit.x);
  EXPECT_GT(yAvg, lowerLimit.y);
  EXPECT_GT(zAvg, lowerLimit.z);
  EXPECT_LT(xAvg, upperLimit.x);
  EXPECT_LT(yAvg, upperLimit.y);
  EXPECT_LT(zAvg, upperLimit.z);
}

void BoschSensorTest::checkDirectChVector(SensorType type, const Event::EventPayload::Vec3& lowerLimit,
                                          const Event::EventPayload::Vec3& upperLimit) {
  std::vector<Event> events;
  useconds_t kCollectionTimeoutUs = 50000 * 1000;  // 50s
  constexpr size_t kNumEvents = 100;
  constexpr size_t kMemSize = 2 * kNumEvents * kEventSize;
  ISensors::SharedMemInfo::SharedMemType memType = ISensors::SharedMemInfo::SharedMemType::ASHMEM;
  SensorInfo sensorInfo{};
  int32_t channelHandle;
  int32_t reportToken = 0;
  constexpr useconds_t SLEEP_GRANULARITY = 1000 * 1000;  // 1s

  for (const SensorInfo& sensor : getSensorsList()) {
    if (sensor.type == type) {
      sensorInfo = sensor;
      break;
    }
  }
  if (!isValidType(sensorInfo.type)) {
    // no default sensor of this type
    return;
  }

  std::shared_ptr<SensorsAidlTestSharedMemory<SensorType, Event>> mem(
    SensorsAidlTestSharedMemory<SensorType, Event>::create(memType, kMemSize));
  ASSERT_NE(mem, nullptr);

  char* buffer = mem->getBuffer();
  size_t size = mem->getSize();

  memset(buffer, 0xff, size);  // Fill the memory with 0xff to make sure it's zeroed after allocation

  ::ndk::ScopedAStatus status = getSensors()->registerDirectChannel(mem->getSharedMemInfo(), &channelHandle);
  ASSERT_TRUE(status.isOk());
  ASSERT_GT(channelHandle, 0);

  // Verify that the memory has been zeroed
  for (size_t i = 0; i < mem->getSize(); i++) {
    ASSERT_EQ(buffer[i], 0x00);
  }

  status =
    getSensors()->configDirectReport(sensorInfo.sensorHandle, channelHandle, ISensors::RateLevel::STOP, &reportToken);
  ASSERT_TRUE(status.isOk());
  status =
    getSensors()->configDirectReport(sensorInfo.sensorHandle, channelHandle, ISensors::RateLevel::NORMAL, &reportToken);
  ASSERT_TRUE(status.isOk());

  while (kCollectionTimeoutUs > 0) {
    useconds_t duration = std::min(SLEEP_GRANULARITY, kCollectionTimeoutUs);
    usleep(duration);
    kCollectionTimeoutUs -= duration;

    events = mem->parseEvents(-1, 0);
    if (events.size() >= kNumEvents) {
      break;
    }
  }
  status =
    getSensors()->configDirectReport(sensorInfo.sensorHandle, channelHandle, ISensors::RateLevel::STOP, &reportToken);
  ASSERT_TRUE(status.isOk());
  ASSERT_GE(events.size(), kNumEvents);
  status = getSensors()->unregisterDirectChannel(channelHandle);

  float xSum = 0.0f;
  float ySum = 0.0f;
  float zSum = 0.0f;
  int validEventCount = 0;

  for (const auto& ev : events) {
    if (ev.payload.getTag() == Event::EventPayload::Tag::vec3) {
      const auto& vec3 = ev.payload.get<Event::EventPayload::Tag::vec3>();
      xSum += vec3.x;
      ySum += vec3.y;
      zSum += vec3.z;
      validEventCount++;
    }
  }
  ASSERT_GT(validEventCount, 0) << "No valid vec3 events collected.";

  const float xAvg = xSum / validEventCount;
  const float yAvg = ySum / validEventCount;
  const float zAvg = zSum / validEventCount;

  ALOGD("Average values, x: %f, y: %f, z: %f", xAvg, yAvg, zAvg);
  EXPECT_GT(xAvg, lowerLimit.x);
  EXPECT_GT(yAvg, lowerLimit.y);
  EXPECT_GT(zAvg, lowerLimit.z);
  EXPECT_LT(xAvg, upperLimit.x);
  EXPECT_LT(yAvg, upperLimit.y);
  EXPECT_LT(zAvg, upperLimit.z);
}

// Test if sensor list contains Bosch sensor
TEST_P(BoschSensorTest, SensorListContainsBoschSensor) {
  std::vector<SensorInfo> sensorInfoList = getSensorsList();
  for (const auto& sensor : sensorInfoList) {
    EXPECT_STREQ(sensor.vendor.c_str(), "Robert Bosch GmbH");
  }
}

// Test if accelerometer sensor vector is in range
TEST_P(BoschSensorTest, AccelerometerCheckSensorVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, GRAVITY_EARTH - 1.0, SensorStatus::ACCURACY_HIGH};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, GRAVITY_EARTH + 1.0, SensorStatus::ACCURACY_HIGH};
  checkVec3Sensor(SensorType::ACCELEROMETER, lowerLimit, upperLimit);
}

// Test if gyroscope sensor vector is in range
TEST_P(BoschSensorTest, GyroscopeCheckSensorVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, -1.0, SensorStatus::ACCURACY_HIGH};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, 1.0, SensorStatus::ACCURACY_HIGH};
  checkVec3Sensor(SensorType::GYROSCOPE, lowerLimit, upperLimit);
}

// Test if gravity sensor vector is in range
TEST_P(BoschSensorTest, GravityCheckSensorVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, GRAVITY_EARTH - 1.0, SensorStatus::ACCURACY_HIGH};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, GRAVITY_EARTH + 1.0, SensorStatus::ACCURACY_HIGH};
  checkVec3Sensor(SensorType::GRAVITY, lowerLimit, upperLimit);
}

// Test if linear acceleration sensor vector is in range
TEST_P(BoschSensorTest, LinearAccelerationCheckSensorVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, -1.0, SensorStatus::ACCURACY_HIGH};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, 1.0, SensorStatus::ACCURACY_HIGH};
  checkVec3Sensor(SensorType::LINEAR_ACCELERATION, lowerLimit, upperLimit);
}

// Test if accelerometer sensor vector is in range with direct channel ASHMEM
TEST_P(BoschSensorTest, AccelerometerCheckDirectChVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, GRAVITY_EARTH - 1.0, SensorStatus::ACCURACY_HIGH};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, GRAVITY_EARTH + 1.0, SensorStatus::ACCURACY_HIGH};
  checkDirectChVector(SensorType::ACCELEROMETER, lowerLimit, upperLimit);
}

// Test if gyroscope sensor vector is in range with direct channel ASHMEM
TEST_P(BoschSensorTest, GyroscopeCheckDirectChVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, -1.0, SensorStatus::ACCURACY_HIGH};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, 1.0, SensorStatus::ACCURACY_HIGH};
  checkDirectChVector(SensorType::GYROSCOPE, lowerLimit, upperLimit);
}

// Test if gravity sensor vector is in range with direct channel ASHMEM
TEST_P(BoschSensorTest, GravityCheckDirectChVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, GRAVITY_EARTH - 1.0, SensorStatus::ACCURACY_HIGH};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, GRAVITY_EARTH + 1.0, SensorStatus::ACCURACY_HIGH};
  checkDirectChVector(SensorType::GRAVITY, lowerLimit, upperLimit);
}

// Test if linear acceleration sensor vector is in range with direct channel ASHMEM
TEST_P(BoschSensorTest, LinearAccelerationCheckDirectChVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, -1.0, SensorStatus::ACCURACY_HIGH};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, 1.0, SensorStatus::ACCURACY_HIGH};
  checkDirectChVector(SensorType::LINEAR_ACCELERATION, lowerLimit, upperLimit);
}

TEST_P(BoschSensorTest, AccelerometerCheckMultiChannelVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, GRAVITY_EARTH - 1.0, SensorStatus::ACCURACY_HIGH};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, GRAVITY_EARTH + 1.0, SensorStatus::ACCURACY_HIGH};
  checkMultiChannelVector(SensorType::ACCELEROMETER, lowerLimit, upperLimit);
}

TEST_P(BoschSensorTest, GyroscopeCheckMultiChannelVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, -1.0};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, 1.0};
  checkMultiChannelVector(SensorType::GYROSCOPE, lowerLimit, upperLimit);
}

TEST_P(BoschSensorTest, LinearAccelerationCheckMultiChannelVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, -1.0};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, 1.0};
  checkMultiChannelVector(SensorType::LINEAR_ACCELERATION, lowerLimit, upperLimit);
}

TEST_P(BoschSensorTest, GravityCheckMultiChannelVector) {
  const Event::EventPayload::Vec3 lowerLimit = {-1.0, -1.0, GRAVITY_EARTH - 1.0};
  const Event::EventPayload::Vec3 upperLimit = {1.0, 1.0, GRAVITY_EARTH + 1.0};
  checkMultiChannelVector(SensorType::GRAVITY, lowerLimit, upperLimit);
}
// Test if sensor name, vendor are as expected
TEST_P(BoschSensorTest, ConfigCheck) {
  const std::regex smiPattern("SMI[0-9]+ BOSCH .* Sensor");
  std::vector<SensorInfo> sensorInfoList = getSensorsList();
  for (const auto& sensor : sensorInfoList) {
    EXPECT_TRUE(std::regex_match(sensor.name.c_str(), smiPattern));
    EXPECT_STREQ(sensor.vendor.c_str(), "Robert Bosch GmbH");
  }
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BoschSensorTest);
INSTANTIATE_TEST_SUITE_P(Sensors, BoschSensorTest,
                         testing::ValuesIn(android::getAidlHalInstanceNames(ISensors::descriptor)),
                         android::PrintInstanceNameToString);
