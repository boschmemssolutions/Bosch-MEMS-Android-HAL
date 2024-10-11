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

#ifndef ANDROID_HARDWARE_BOSCH_SENSOR_LIST_H
#define ANDROID_HARDWARE_BOSCH_SENSOR_LIST_H

#include <memory>
#include <vector>

#include "SMI230.h"
#include "SMI240.h"
#include "SMI330.h"
#include "SensorCore.h"

namespace bosch {
namespace sensors {

class SensorList {
public:
  std::vector<std::shared_ptr<ISensorHal>> getAvailableSensors();

private:
  std::vector<std::shared_ptr<SensorCore>> mSensorList{std::make_shared<Smi330Acc>(), std::make_shared<Smi330Gyro>(),
                                                       std::make_shared<Smi240Acc>(), std::make_shared<Smi240Gyro>(),
                                                       std::make_shared<Smi230Acc>(), std::make_shared<Smi230Gyro>()};

  std::vector<std::shared_ptr<CompositeSensorCore>> mCompositeSensorList{
    std::make_shared<Smi330Gravity>(mSensorList[0], mSensorList[1]),
    std::make_shared<Smi330LinearAcc>(mSensorList[0], mSensorList[1]),
    std::make_shared<Smi240Gravity>(mSensorList[2], mSensorList[3]),
    std::make_shared<Smi240LinearAcc>(mSensorList[2], mSensorList[3]),
    std::make_shared<Smi230Gravity>(mSensorList[4], mSensorList[5]),
    std::make_shared<Smi230LinearAcc>(mSensorList[4], mSensorList[5])};
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSOR_LIST_H
