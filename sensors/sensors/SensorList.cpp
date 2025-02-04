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

#include "SensorList.h"

#include "FileHandler.h"

namespace bosch::sensors {

std::vector<std::shared_ptr<ISensorHal>> SensorList::getAvailableSensors() {
  std::string device{};
  std::vector<std::shared_ptr<ISensorHal>> availableSensors{};

  for (const auto& sensor : mSensorList) {
    if (bosch::hwctl::isSensorAvailable(sensor->getSensorData().driverName, device)) {
      sensor->setAvailable(true);
      sensor->setDevice(device);
      availableSensors.push_back(sensor);
    }
  }

  for (const auto& compositeSensor : mCompositeSensorList) {
    int dependentSensors = 0;
    for (const auto& sensor : compositeSensor->getDependencyList()) {
      if (sensor->isAvailable()) {
        dependentSensors++;
      }
    }
    if (dependentSensors == compositeSensor->getDependencyList().size()) {
      availableSensors.push_back(compositeSensor);
    }
  }

  return availableSensors;
};

}  // namespace bosch::sensors