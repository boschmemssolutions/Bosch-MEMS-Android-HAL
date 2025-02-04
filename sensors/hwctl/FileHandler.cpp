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

#include "FileHandler.h"

#include <dirent.h>

#include <regex>

namespace bosch::hwctl {

ReadHandler::ReadHandler(const std::string& path, const std::string& file) : mFstream(path + file) {}

ReadHandler::~ReadHandler() {
  if (mFstream.is_open()) mFstream.close();
}

int ReadHandler::read(std::string& result) {
  std::lock_guard<std::mutex> lock(mSysfsMutex);
  if (!mFstream.is_open()) return -1;

  mFstream.clear();
  mFstream.seekg(0);
  result.assign((std::istreambuf_iterator<char>(mFstream)), (std::istreambuf_iterator<char>()));

  return 0;
}

WriteHandler::WriteHandler(const std::string& path, const std::string& file) : mFstream(path + file) {}

WriteHandler::WriteHandler(const std::string& path, const std::string& file, const std::string& content)
  : mFstream(path + file) {
  write(content);
}

WriteHandler::~WriteHandler() {
  if (mFstream.is_open()) mFstream.close();
}

int WriteHandler::write(const std::string& content) {
  std::lock_guard<std::mutex> lock(mSysfsMutex);
  if (!mFstream.is_open()) return -1;

  mFstream << content;

  if (mFstream.fail()) return -1;

  return 0;
}

void RawSysfsHandler::init(const std::string& path, const std::array<std::string, 3>& files) {
  for (const auto& file : files) {
    if (file.empty()) break;
    mFileHandlers.push_back(std::make_unique<ReadHandler>(path, file));
  }
}

int RawSysfsHandler::read(std::vector<float>& results, float resolution) {
  for (auto& handler : mFileHandlers) {
    std::string content;
    const int status = handler->read(content);
    if (status != 0) return status;
    if (!isValidInteger(content)) return -1;

    results.push_back(std::stoi(content) * resolution);
  }

  return 0;
}

bool RawSysfsHandler::isValidInteger(const std::string& s) const {
  static const std::regex number_regex(R"(^\s*[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?\s*$)");
  return std::regex_match(s, number_regex);
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
      ReadHandler fileHandler(iioPath, name_path);
      if (0 == fileHandler.read(name)) {
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

}  // namespace bosch::hwctl
