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

#include "SensorCore.h"

#include <fcntl.h>
#include <log/log.h>
#include <utils/SystemClock.h>

#include <algorithm>
#include <sstream>

#include "hwctl.h"

using namespace bosch::sensors;

void SensorCore::activate(bool enable) { activateByType(mSensorData.type, enable); }

void SensorCore::activateByType(BoschSensorType type, bool enable) {
  mEnableState[type] = enable;

  const bool isEnabled =
    std::any_of(mEnableState.begin(), mEnableState.end(), [](const auto& state) { return state.second; });

  if (isEnabled != mIsEnabled) {
    mIsEnabled = isEnabled;
    setPowerMode(isEnabled);
    updateSamplingRate();
  }
}

void SensorCore::batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
  batchByType(mSensorData.type, samplingPeriodNs, maxReportLatencyNs);
}

void SensorCore::batchByType(BoschSensorType type, int64_t samplingPeriodNs, int64_t) {
  mSamplingPeriods[type] = samplingPeriodNs;
  updateSamplingRate();
}

void SensorCore::updateSamplingRate() {
  int64_t usedSamplingPeriod = mSensorData.maxDelayUs * 1000;

  for (const auto& samplingPeriod : mSamplingPeriods) {
    if (mEnableState[samplingPeriod.first] && (samplingPeriod.second < usedSamplingPeriod)) {
      usedSamplingPeriod = samplingPeriod.second;
    }
  }

  setSamplingRate(usedSamplingPeriod);
}

std::vector<SensorValues> SensorCore::readSensorValues() {
  std::vector<SensorValues> sensorValues{};
  readPollingData(sensorValues);
  return sensorValues;
}

void SensorCore::readPollingData(std::vector<SensorValues>& values) {
  SensorValues value{};
  value.timestamp = ::android::elapsedRealtimeNano();

  for (const auto& sysfs : mSensorData.sysfsRaw) {
    std::string data;
    if (0 == hwctl::readFromFile(mDevice + sysfs, data)) {
      value.data.push_back(::atof(data.c_str()) * mSensorData.resolution);
    } else {
      ALOGE("Sensor readPollingData failed");
      return;
    }
  }

  values.push_back(value);
}
