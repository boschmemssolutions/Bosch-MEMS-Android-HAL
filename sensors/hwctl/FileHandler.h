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

#pragma once

#include <array>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace bosch::hwctl {

class ReadHandler {
public:
  ReadHandler(const std::string& path, const std::string& file);
  ~ReadHandler();

  int read(std::string& result);

private:
  std::ifstream mFstream;
  std::mutex mSysfsMutex;
};

class WriteHandler {
public:
  WriteHandler(const std::string& path, const std::string& file);
  WriteHandler(const std::string& path, const std::string& file, const std::string& content);
  ~WriteHandler();

  int write(const std::string& content);

private:
  std::ofstream mFstream;
  std::mutex mSysfsMutex;
};

class RawSysfsHandler {
public:
  void init(const std::string& path, const std::array<std::string, 3>& files);
  int read(std::vector<float>& results, float resolution);

private:
  bool isValidInteger(const std::string& s) const;

  std::vector<std::unique_ptr<ReadHandler>> mFileHandlers{};
};

bool isSensorAvailable(const std::string& driverName, std::string& device);

}  // namespace bosch::hwctl
