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

#ifndef ANDROID_HARDWARE_BOSCH_ACCELEROMTER_H
#define ANDROID_HARDWARE_BOSCH_ACCELEROMTER_H

#include <functional>
#include <map>
#include <string>
#include <unordered_map>

#include "ISensorThread.h"
#include "SensorCore.h"

namespace bosch {
namespace sensors {

struct AccelData {
  std::string sysfsEnable;
  std::string sysfsDisable;
  std::string sysfsTrigger;
  AccelRange defaultRange;
  AccelRange minRange;
  AccelRange maxRange;
  std::map<int64_t, std::string> odrMap;
};

class Accelerometer : public SensorCore {
public:
  Accelerometer(const std::shared_ptr<ISensorThread> sensorThread, const AccelData& accelData);
  ~Accelerometer() = default;

  void init(int deviceNum) override;
  void enableSensor(bool enable) override;
  void setSamplingRate(int64_t samplingPeriodNs, int64_t reportLatencyNs) override;
  void setOdr(int64_t samplingPeriodNs);

private:
  void setScale();
  void checkAndUpdateRange();

  const std::shared_ptr<ISensorThread> mSensorThread;
  const AccelData& mAccelData;

  const std::string mSysfsPower = "in_accel_en";
  const std::string mSysfsOdr = "in_accel_sampling_frequency";
  const std::string mSysfsScale = "in_accel_scale";
  const std::unordered_map<AccelRange, std::string> mRangeMap = {{AccelRange::RANGE_2G, "0.000061035"},
                                                                 {AccelRange::RANGE_4G, "0.000122070"},
                                                                 {AccelRange::RANGE_8G, "0.000244140"},
                                                                 {AccelRange::RANGE_16G, "0.000488281"}};
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_ACCELEROMTER_H