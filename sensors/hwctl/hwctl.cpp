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

#include "hwctl.h"

#include <dirent.h>

#include <fstream>
#include <mutex>
#include <regex>
#include <string>

namespace hwctl {

std::mutex sysfsMutex;

int32_t readFromFile(const std::string& file, std::string& result) {
  std::lock_guard<std::mutex> lock(sysfsMutex);
  std::ifstream ifs(file.c_str());

  if (!ifs.is_open()) {
    return -1;
  }
  result.assign((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

  ifs.close();
  return 0;
}

int32_t writeToFile(const std::string& writeString) {
  std::string path;
  std::string content;
  size_t spacePos = writeString.find(' ');
  if (spacePos != std::string::npos) {
    path = writeString.substr(0, spacePos);
    content = writeString.substr(spacePos + 1);
  } else {
    return -1;
  }

  std::lock_guard<std::mutex> lock(sysfsMutex);
  std::ofstream ofs(path.c_str());

  if (!ofs.is_open()) {
    return -1;
  }

  ofs << content;

  if (ofs.fail()) {
    ofs.close();
    return -1;
  }

  ofs.close();
  return 0;
}

bool isSensorAvailable(const std::string& driverName, std::string& device) {
  const std::string iioPath = "/sys/bus/iio/devices/";
  const std::regex pattern("iio:device\\d+");
  bool sensorFound = false;
  std::string name = {};

  DIR* dir = opendir(iioPath.c_str());
  if (dir == NULL) {
    return false;
  }

  for (dirent* entry = readdir(dir); entry != NULL; entry = readdir(dir)) {
    if (std::regex_match(entry->d_name, pattern)) {
      const std::string name_path = std::string(entry->d_name) + "/name";
      if (0 == readFromFile(iioPath + name_path, name)) {
        if (name.compare(0, driverName.size(), driverName) == 0) {
          device = iioPath + std::string(entry->d_name) + "/";
          sensorFound = true;
          break;
        }
      }
    }
  }

  closedir(dir);
  return sensorFound;
}

}  // namespace hwctl