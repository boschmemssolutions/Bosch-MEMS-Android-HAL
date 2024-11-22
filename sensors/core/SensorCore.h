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

#ifndef ANDROID_HARDWARE_BOSCH_SENSOR_CORE_H
#define ANDROID_HARDWARE_BOSCH_SENSOR_CORE_H

#include <cmath>
#include <map>

#include "ISensorHal.h"

namespace bosch {
namespace sensors {

constexpr float gravityToAcceleration(float gravity) { return gravity * 9.80665f; }
constexpr float degreeToRad(float degree) { return degree * M_PI / 180.0f; }

class SensorCore : public ISensorHal {
public:
  SensorCore() = default;
  ~SensorCore() override = default;

  std::vector<SensorValues> readSensorValues() override;
  bool readSensorTemperature(float* temperature) override;
  void activate(bool enable) override;
  void batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) override;
  const SensorData& getSensorData() const override { return mSensorData; }

  void setDevice(const std::string& device) { mDevice = device; }
  void setAvailable(bool available) { mAvailable = available; }
  bool isAvailable() const { return mAvailable; }

  void activateByType(BoschSensorType type, bool enable);
  void batchByType(BoschSensorType type, int64_t samplingPeriodNs, int64_t maxReportLatencyNs);

protected:
  virtual void setPowerMode(bool enable) { (void)enable; };
  virtual void setSamplingRate(int64_t samplingPeriodNs) { (void)samplingPeriodNs; };

  std::string mDevice{};
  SensorData mSensorData{};

private:
  void updateSamplingRate();
  void readPollingData(std::vector<SensorValues>& values);

  bool mAvailable{false};
  bool mIsEnabled{false};
  std::map<BoschSensorType, bool> mEnableState{};
  std::map<BoschSensorType, int64_t> mSamplingPeriods{};
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSOR_CORE_H
