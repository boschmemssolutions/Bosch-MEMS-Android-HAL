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

#include "FileHandler.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <log/log.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>

namespace bosch::hwctl {

static std::string trim(const std::string& s) noexcept {
  auto start = std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); });
  auto end = std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base();
  return (start < end) ? std::string(start, end) : std::string();
}

ReadHandler::ReadHandler(const std::string& path, const std::string& file) noexcept : mFilePath(path + file) {}

int ReadHandler::read(std::string& result) noexcept {
  std::ifstream f(mFilePath);
  if (!f.is_open()) {
    ALOGE("ReadHandler: Failed to open %s (errno: %d)", mFilePath.c_str(), errno);
    return -1;
  }

  result.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (f.fail() && !f.eof()) {
    ALOGE("ReadHandler: Error reading from %s", mFilePath.c_str());
    return -1;
  }
  result = trim(result);

  return 0;
}

WriteHandler::WriteHandler(const std::string& path, const std::string& file) noexcept : mFilePath(path + file) {}

WriteHandler::WriteHandler(const std::string& path, const std::string& file, const std::string& content) noexcept
  : mFilePath(path + file) {
  const int status = write(content);
  if (status != 0) {
    ALOGE("WriteHandler: Failed to write during construction to %s", mFilePath.c_str());
  }
}

int WriteHandler::write(const std::string& content) noexcept {
  std::ofstream f(mFilePath);
  if (!f.is_open()) {
    ALOGE("WriteHandler: Failed to open %s for writing (errno: %d)", mFilePath.c_str(), errno);
    return -1;
  }

  f << content;
  f.flush();
  if (f.fail()) {
    ALOGE("WriteHandler: Failed to write to %s", mFilePath.c_str());
    return -1;
  }

  return 0;
}

WriteReadbackHandler::WriteReadbackHandler(const std::string& path, const std::string& file) noexcept
  : mPath(path), mFile(file) {}

WriteReadbackHandler::WriteReadbackHandler(const std::string& path, const std::string& file,
                                           const std::string& content) noexcept
  : mPath(path), mFile(file) {
  const int status = writeAndRead(content);
  if (status != 0) {
    ALOGE("WriteReadbackHandler: Failed to write and verify '%s' to %s", content.c_str(), (path + file).c_str());
  }
}

int WriteReadbackHandler::writeAndRead(const std::string& content) noexcept {
  {
    WriteHandler handler(mPath, mFile);
    const int status = handler.write(content);
    if (status != 0) return status;
  }

  std::string readback;
  ReadHandler handler(mPath, mFile);
  for (int i = 0; i < kMaxRetries; ++i) {
    const int status = handler.read(readback);
    if ((status == 0) && (readback == content)) return 0;
    ALOGW("WriteReadbackHandler: Mismatch on attempt %d for %s (wrote: '%s', read: '%s')", i + 1,
          (mPath + mFile).c_str(), content.c_str(), readback.c_str());
    usleep(kRetryDelayUs);
  }
  return -1;
}

static bool isIioDeviceAvailable(const std::string& deviceName, const std::string& prefix, int* deviceNum) noexcept {
  if (deviceNum == nullptr) {
    ALOGE("isIioDeviceAvailable: deviceNum pointer is null");
    return false;
  }
  if (deviceName.empty()) {
    ALOGE("isIioDeviceAvailable: deviceName is empty");
    return false;
  }

  *deviceNum = -1;
  const std::string iioPath = "/sys/bus/iio/devices/";
  std::string name;
  const size_t prefixLen = prefix.length();

  struct DirCloser {
    DIR* dir;
    explicit DirCloser(DIR* d) : dir(d) {}
    ~DirCloser() {
      if (dir) closedir(dir);
    }
    DIR* get() const { return dir; }
  };

  DirCloser dirCloser(opendir(iioPath.c_str()));
  DIR* dir = dirCloser.get();
  if (!dir) {
    ALOGE("isIioDeviceAvailable: Failed to open %s (errno: %d)", iioPath.c_str(), errno);
    return false;
  }

  for (dirent* entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
    const char* dname = entry->d_name;

    if (std::strncmp(dname, prefix.c_str(), prefixLen) != 0) continue;

    const char* numPart = dname + prefixLen;
    if (*numPart == '\0') continue;

    char* endPtr = nullptr;
    errno = 0;
    const int64_t devNum = std::strtol(numPart, &endPtr, 10);

    if (endPtr == numPart || *endPtr != '\0' || errno == ERANGE || devNum < 0) continue;

    const std::string namePath = std::string(dname) + "/name";
    ReadHandler fileHandler(iioPath, namePath);

    if (fileHandler.read(name) == 0 && !name.empty()) {
      if (name.compare(0, deviceName.size(), deviceName) == 0) {
        *deviceNum = static_cast<int>(devNum);
        return true;
      }
    }
  }

  return false;
}

bool isSensorAvailable(const std::string& driverName, int* deviceNum) noexcept {
  return isIioDeviceAvailable(driverName, "iio:device", deviceNum);
}

bool isTriggerAvailable(const std::string& triggerName, int* triggerNum) noexcept {
  return isIioDeviceAvailable(triggerName, "trigger", triggerNum);
}

int createDirectory(const std::string& path) noexcept {
  if (path.empty()) {
    ALOGE("createDirectory: path is empty");
    return -1;
  }

  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      return 0;
    } else {
      ALOGE("createDirectory: %s exists but is not a directory", path.c_str());
      return -1;
    }
  }

  if (mkdir(path.c_str(), 0755) != 0) {
    ALOGE("createDirectory: Failed to create %s (errno: %d)", path.c_str(), errno);
    return -1;
  }

  return 0;
}

}  // namespace bosch::hwctl
