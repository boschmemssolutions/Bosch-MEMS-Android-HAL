/*
 * Copyright (C) 2024-2026 Robert Bosch GmbH
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

#ifndef ANDROID_HARDWARE_BOSCH_GYROSCOPE_H
#define ANDROID_HARDWARE_BOSCH_GYROSCOPE_H

#include <functional>
#include <map>
#include <string>
#include <unordered_map>

#include "ISensorThread.h"
#include "SensorCore.h"

namespace bosch {
namespace sensors {

struct GyroData {
  std::string sysfsEnable;
  std::string sysfsDisable;
  std::string sysfsTrigger;
  GyroRange defaultRange;
  GyroRange minRange;
  GyroRange maxRange;
  std::map<int64_t, std::string> odrMap;
  bool powerOnForChange;
};

class Gyroscope : public SensorCore {
public:
  Gyroscope(const std::shared_ptr<ISensorThread> sensorThread, const GyroData& gyroData);
  ~Gyroscope() = default;

  void init(int deviceNum) override;
  void enableSensor(bool enable) override;
  void setSamplingRate(int64_t samplingPeriodNs, int64_t reportLatencyNs) override;
  void setOdr(int64_t samplingPeriodNs);

private:
  void writeWithPowerCheck(const std::string& file, const std::string& content);
  void setScale();
  void checkAndUpdateRange();

  const std::shared_ptr<ISensorThread> mSensorThread;
  const GyroData& mGyroData;

  const std::string mSysfsPower = "in_anglvel_en";
  const std::string mSysfsOdr = "in_anglvel_sampling_frequency";
  const std::string mSysfsScale = "in_anglvel_scale";
  const std::unordered_map<GyroRange, std::string> mRangeMap = {{GyroRange::RANGE_125_DEG_S, "0.003814697"},
                                                                {GyroRange::RANGE_250_DEG_S, "0.007629395"},
                                                                {GyroRange::RANGE_500_DEG_S, "0.015258789"},
                                                                {GyroRange::RANGE_1000_DEG_S, "0.030517578"},
                                                                {GyroRange::RANGE_2000_DEG_S, "0.061035156"}};
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_GYROSCOPE_H