/*
 * Copyright (C) 2023 Robert Bosch GmbH
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

enum BoschSensorType {
  ACCEL = 1,                 // SensorType::ACCELEROMETER
  GYRO = 4,                  // SensorType::GYROSCOPE
  GRAVITY = 9,               // SensorType::GRAVITY
  LINEAR_ACCEL = 10,         // SensorType::LINEAR_ACCELERATION
  AMBIENT_TEMPERATURE = 13,  // SensorType::AMBIENT_TEMPERATURE
};

enum SensorReportingMode {
  CONTINUOUS = 0,
  ON_CHANGE = 1,
  ONE_SHOT = 2,
  SPECIAL_REPORTING = 3,
};

struct SensorData {
  std::string vendor{"Robert Bosch GmbH"};
  std::string driverName;
  std::string sensorName;
  std::array<std::string, 3> sysfsRaw;
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
};

struct SensorValues {
  int64_t timestamp;
  std::vector<float> data;
};

class ISensorHal {
public:
  ISensorHal() = default;
  virtual ~ISensorHal() = default;

  virtual std::vector<SensorValues> readSensorValues() = 0;
  virtual bool readSensorTemperature(float* temperature) = 0;
  virtual void activate(bool enable) = 0;
  virtual void batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) = 0;
  virtual const SensorData& getSensorData() const = 0;
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_ISENSOR_HAL_H
