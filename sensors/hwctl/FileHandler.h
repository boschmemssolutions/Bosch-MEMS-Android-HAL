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

#pragma once

#include <array>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace bosch::hwctl {

class ReadHandler {
public:
  ReadHandler(const std::string& path, const std::string& file) noexcept;
  ~ReadHandler() noexcept = default;

  int read(std::string& result) noexcept;

private:
  std::string mFilePath;
};

class WriteHandler {
public:
  WriteHandler(const std::string& path, const std::string& file) noexcept;
  WriteHandler(const std::string& path, const std::string& file, const std::string& content) noexcept;
  ~WriteHandler() noexcept = default;

  int write(const std::string& content) noexcept;

private:
  std::string mFilePath;
};

class WriteReadbackHandler {
public:
  WriteReadbackHandler(const std::string& path, const std::string& file) noexcept;
  WriteReadbackHandler(const std::string& path, const std::string& file, const std::string& content) noexcept;
  ~WriteReadbackHandler() noexcept = default;

  int writeAndRead(const std::string& content) noexcept;

private:
  std::string mPath;
  std::string mFile;
  static constexpr int kMaxRetries = 5;
  static constexpr useconds_t kRetryDelayUs = 1000;
};

bool isSensorAvailable(const std::string& driverName, int* deviceNum) noexcept;
bool isTriggerAvailable(const std::string& triggerName, int* triggerNum) noexcept;
int createDirectory(const std::string& path) noexcept;

}  // namespace bosch::hwctl
