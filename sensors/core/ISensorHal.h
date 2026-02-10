/*
 * Copyright (C) 2024 Robert Bosch GmbH
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

#ifndef ANDROID_HARDWARE_BOSCH_ISENSOR_HAL_H
#define ANDROID_HARDWARE_BOSCH_ISENSOR_HAL_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bosch {
namespace sensors {

/*
 * Accept sensor data with timestamp diffs > factor * expected diff.
 */
constexpr float VALID_TIMESTAMP_FACTOR = 0.9f;

constexpr float MIN_VALID_TIMESTAMP_FACTOR = 0.5f;

enum BoschSensorType {
  INVALID = -1,
  META_DATA = 0,            // SensorType::META_DATA
  ACCEL = 1,                // SensorType::ACCELEROMETER
  GYRO = 4,                 // SensorType::GYROSCOPE
  GRAVITY = 9,              // SensorType::GRAVITY
  LINEAR_ACCEL = 10,        // SensorType::LINEAR_ACCELERATION
  GYRO_UNCALIBRATED = 16,   // SensorType::GYROSCOPE_UNCALIBRATED
  ACCEL_UNCALIBRATED = 35,  // SensorType::ACCELEROMETER_UNCALIBRATED
};

enum SensorReportingMode {
  CONTINUOUS = 0,
  ON_CHANGE = 1,
  ONE_SHOT = 2,
  SPECIAL_REPORTING = 3,
};

enum class ScanType {
  DRDY,
  HW_FIFO,
  SW_TIMER,
};

enum class AccelRange {
  RANGE_2G = 2,
  RANGE_4G = 4,
  RANGE_8G = 8,
  RANGE_16G = 16,
};

enum class GyroRange {
  RANGE_125_DEG_S = 125,
  RANGE_250_DEG_S = 250,
  RANGE_500_DEG_S = 500,
  RANGE_1000_DEG_S = 1000,
  RANGE_2000_DEG_S = 2000,
};

struct SensorData {
  std::string vendor{"Robert Bosch GmbH"};
  std::string driverName;
  std::string sensorName;
  std::string propertyIdentifier;
  std::string temperatureSysfsRaw;
  BoschSensorType type;
  int32_t minDelayUs;
  int32_t maxDelayUs;
  float power;
  float range;
  float resolution;
  float temperatureScale;
  float temperatureOffset;
  SensorReportingMode reportMode;
  enum ScanType scanType;
};

struct SensorValues {
  BoschSensorType type;
  bool flushRequest;
  int64_t timestamp;
  std::vector<float> data;
};

class ISensorValues {
public:
  ISensorValues() = default;
  virtual ~ISensorValues() = default;

  virtual void update(const std::vector<SensorValues>& values) = 0;
};

class ICompositeObserverCb : public ISensorValues {
public:
  ICompositeObserverCb() = default;
  virtual ~ICompositeObserverCb() = default;

  virtual void updateValidTimestampFactor(int64_t odrNs) = 0;
};

class ISensorHal {
public:
  ISensorHal() = default;
  virtual ~ISensorHal() = default;

  virtual void registerObserver(const std::shared_ptr<ISensorValues>& observer) = 0;
  virtual void removeObserver(const std::shared_ptr<ISensorValues>& observer) = 0;
  virtual bool readSensorTemperature(float* temperature) = 0;
  virtual void activate(bool enable) = 0;
  virtual void batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) = 0;
  virtual void flush() = 0;
  virtual const SensorData& getSensorData() const = 0;
  virtual const std::array<float, 9>& getRotationMatrix() const = 0;
  virtual const std::array<float, 3>& getLocationVector() const = 0;
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_ISENSOR_HAL_H
