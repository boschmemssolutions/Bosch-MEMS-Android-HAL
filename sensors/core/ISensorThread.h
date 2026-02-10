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

#ifndef ANDROID_HARDWARE_BOSCH_ISENSOR_THREAD_H
#define ANDROID_HARDWARE_BOSCH_ISENSOR_THREAD_H

#include <array>
#include <functional>
#include <string>

#include "ISensorHal.h"

namespace bosch {
namespace sensors {

enum SensorTypeIndex { ACCEL_IDX, GYRO_IDX, IDX_LENGTH };

class ISensorThread {
public:
  virtual ~ISensorThread() = default;

  virtual void init(SensorTypeIndex idx, int deviceNum, std::function<void(int64_t)> odrCallback, ScanType scanType,
                    const std::array<float, 9>& rotationMatrix, const std::string& drdyTrigger = "") = 0;
  virtual void setResolution(SensorTypeIndex idx, float resolution) = 0;
  virtual void registerObserver(SensorTypeIndex idx, const std::shared_ptr<ISensorValues>& observer) = 0;
  virtual void removeObserver(SensorTypeIndex idx, const std::shared_ptr<ISensorValues>& observer) = 0;
  virtual void enable(SensorTypeIndex idx, bool enable, const std::string& sysfsPwr, const std::string& mode) = 0;
  virtual void setSamplingRate(SensorTypeIndex idx, int64_t samplingPeriodNs, int64_t reportLatencyNs) = 0;
  virtual void flush() = 0;
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_ISENSOR_THREAD_H
