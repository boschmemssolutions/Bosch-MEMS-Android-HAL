/*
 * Copyright (C) 2024-2025 Robert Bosch GmbH
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
#include "SensorThread.h"

namespace bosch {
namespace sensors {

class SensorList {
public:
  std::vector<std::shared_ptr<ISensorHal>> getAvailableSensors();

private:
  enum SensorThreadIndex { SMI330_THREAD, SMI240_THREAD, SMI230_ACC_THREAD, SMI230_GYRO_THREAD };
  enum SensorCoreIndex { SMI330_ACC, SMI330_GYRO, SMI240_ACC, SMI240_GYRO, SMI230_ACC, SMI230_GYRO };

  std::vector<std::shared_ptr<ISensorThread>> mSensorThread{
    std::make_shared<SensorThreadCombined>(),  // SMI330_THREAD (ACC and GYRO combined)
    std::make_shared<SensorThreadCombined>(),  // SMI240_THREAD (ACC and GYRO combined)
    std::make_shared<SensorThreadSeparate>(),  // SMI230_ACC_THREAD
    std::make_shared<SensorThreadSeparate>()   // SMI230_GYRO_THREAD
  };

  std::vector<std::shared_ptr<SensorCore>> mSensorList{std::make_shared<Smi330Acc>(mSensorThread[SMI330_THREAD]),
                                                       std::make_shared<Smi330Gyro>(mSensorThread[SMI330_THREAD]),
                                                       std::make_shared<Smi240Acc>(mSensorThread[SMI240_THREAD]),
                                                       std::make_shared<Smi240Gyro>(mSensorThread[SMI240_THREAD]),
                                                       std::make_shared<Smi230Acc>(mSensorThread[SMI230_ACC_THREAD]),
                                                       std::make_shared<Smi230Gyro>(mSensorThread[SMI230_GYRO_THREAD])};

  std::vector<std::shared_ptr<CompositeSensorCore>> mCompositeSensorList{
    std::make_shared<Smi330AccUncalibrated>(mSensorList[SMI330_ACC]),
    std::make_shared<Smi330GyroUncalibrated>(mSensorList[SMI330_GYRO]),
    std::make_shared<Smi330Gravity>(mSensorList[SMI330_ACC], mSensorList[SMI330_GYRO]),
    std::make_shared<Smi330LinearAcc>(mSensorList[SMI330_ACC], mSensorList[SMI330_GYRO]),
    std::make_shared<Smi240AccUncalibrated>(mSensorList[SMI240_ACC]),
    std::make_shared<Smi240GyroUncalibrated>(mSensorList[SMI240_GYRO]),
    std::make_shared<Smi240Gravity>(mSensorList[SMI240_ACC], mSensorList[SMI240_GYRO]),
    std::make_shared<Smi240LinearAcc>(mSensorList[SMI240_ACC], mSensorList[SMI240_GYRO]),
    std::make_shared<Smi230AccUncalibrated>(mSensorList[SMI230_ACC]),
    std::make_shared<Smi230GyroUncalibrated>(mSensorList[SMI230_GYRO]),
    std::make_shared<Smi230Gravity>(mSensorList[SMI230_ACC], mSensorList[SMI230_GYRO]),
    std::make_shared<Smi230LinearAcc>(mSensorList[SMI230_ACC], mSensorList[SMI230_GYRO])};
};

}  // namespace sensors
}  // namespace bosch

#endif  // ANDROID_HARDWARE_BOSCH_SENSOR_LIST_H
