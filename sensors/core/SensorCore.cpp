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

#include "SensorCore.h"

#include <cutils/properties.h>
#include <fcntl.h>
#include <log/log.h>

#include <algorithm>
#include <sstream>

#include "FileHandler.h"

using namespace bosch::sensors;

SensorCore::SensorCore(const std::shared_ptr<ISensorThread> sensorThread) : mSensorThread(sensorThread) {
  mRunThread = std::thread(startThread, this);
}

SensorCore::~SensorCore() {
  mStopThread.store(true);
  mWaitCV.notify_all();
  mRunThread.join();
}

void SensorCore::initBase(int deviceNum) {
  mAvailable = true;
  mDevice = "/sys/bus/iio/devices/iio:device" + std::to_string(deviceNum) + "/";

  readRotationMatrix();
  readLocationVector();

  batch(mSensorData.minDelayUs * 1000, 0);  // Set default batch parameters
}

void SensorCore::readRotationMatrix() {
  char rotationMatrixBuf[128] = {0};
  const std::string rotationMatrixProp =
    "persist.vendor.bosch.sensor." + mSensorData.propertyIdentifier + ".rotation_matrix.config";

  property_get(rotationMatrixProp.c_str(), rotationMatrixBuf, "1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0");

  std::stringstream ss(rotationMatrixBuf);
  for (size_t i = 0; i < mRotationMatrix.size() && ss >> mRotationMatrix[i]; ++i) {
    if (ss.peek() == ',') ss.ignore();
  }
}

void SensorCore::readLocationVector() {
  char locationBuf[128] = {0};
  const std::string locationProp =
    "persist.vendor.bosch.sensor." + mSensorData.propertyIdentifier + ".location_vector.config";

  property_get(locationProp.c_str(), locationBuf, "0.0,0.0,0.0");

  std::stringstream ss(locationBuf);
  for (size_t i = 0; i < mLocationVector.size() && ss >> mLocationVector[i]; ++i) {
    if (ss.peek() == ',') ss.ignore();
  }
}

ScanType SensorCore::readScanType(ScanType defaultScanType) {
  char scanTypeBuf[128] = {0};
  const std::string scanTypeProp =
    "persist.vendor.bosch.sensor." + mSensorData.propertyIdentifier + ".scan_type.config";

  property_get(scanTypeProp.c_str(), scanTypeBuf, "");

  const std::string scanTypeStr(scanTypeBuf);
  if (scanTypeStr == "drdy") {
    return ScanType::DRDY;
  } else if (scanTypeStr == "hw_fifo") {
    return ScanType::HW_FIFO;
  } else if (scanTypeStr == "sw_timer") {
    return ScanType::SW_TIMER;
  }
  return defaultScanType;
}

AccelRange SensorCore::readAccelRange(AccelRange defaultRange) {
  const std::string rangeProp = "persist.vendor.bosch.sensor." + mSensorData.propertyIdentifier + ".acc_range.config";
  const int range = property_get_int32(rangeProp.c_str(), static_cast<int32_t>(defaultRange));
  return static_cast<AccelRange>(range);
}

GyroRange SensorCore::readGyroRange(GyroRange defaultRange) {
  const std::string rangeProp = "persist.vendor.bosch.sensor." + mSensorData.propertyIdentifier + ".gyro_range.config";
  const int range = property_get_int32(rangeProp.c_str(), static_cast<int32_t>(defaultRange));
  return static_cast<GyroRange>(range);
}

void SensorCore::registerObserver(const std::shared_ptr<ISensorValues>& observer) {
  std::lock_guard<std::mutex> lock(mRunMutex);
  mObservers.push_back(observer);
}

void SensorCore::registerCompositeObserver(const std::shared_ptr<ICompositeObserverCb>& observer) {
  const SensorTypeIndex idx = mSensorData.type == BoschSensorType::ACCEL ? ACCEL_IDX : GYRO_IDX;
  mSensorThread->registerObserver(idx, observer);

  std::lock_guard<std::mutex> lock(mRunMutex);
  mCompositeObservers.push_back(observer);
}

void SensorCore::removeObserver(const std::shared_ptr<ISensorValues>& observer) {
  std::lock_guard<std::mutex> lock(mRunMutex);
  mObservers.erase(
    std::remove_if(mObservers.begin(), mObservers.end(),
                   [observer](const std::weak_ptr<ISensorValues>& obs) { return obs.lock() == observer; }),
    mObservers.end());
}

void SensorCore::removeCompositeObserver(const std::shared_ptr<ICompositeObserverCb>& observer) {
  const SensorTypeIndex idx = mSensorData.type == BoschSensorType::ACCEL ? ACCEL_IDX : GYRO_IDX;
  mSensorThread->removeObserver(idx, observer);

  std::lock_guard<std::mutex> lock(mRunMutex);
  mCompositeObservers.erase(
    std::remove_if(mCompositeObservers.begin(), mCompositeObservers.end(),
                   [observer](const std::weak_ptr<ICompositeObserverCb>& obs) { return obs.lock() == observer; }),
    mCompositeObservers.end());
}

void SensorCore::cleanAndCopyObservers(std::vector<std::weak_ptr<ISensorValues>>& observersCopy) {
  mObservers.erase(std::remove_if(mObservers.begin(), mObservers.end(),
                                  [&observersCopy](const std::weak_ptr<ISensorValues>& obs) {
                                    std::shared_ptr<ISensorValues> sharedObs = obs.lock();
                                    if (sharedObs) {
                                      observersCopy.push_back(obs);
                                      return false;
                                    }
                                    return true;
                                  }),
                   mObservers.end());
}

void SensorCore::cleanAndCopyCompositeObservers(std::vector<std::weak_ptr<ICompositeObserverCb>>& observersCopy) {
  mCompositeObservers.erase(std::remove_if(mCompositeObservers.begin(), mCompositeObservers.end(),
                                           [&observersCopy](const std::weak_ptr<ICompositeObserverCb>& obs) {
                                             std::shared_ptr<ICompositeObserverCb> sharedObs = obs.lock();
                                             if (sharedObs) {
                                               observersCopy.push_back(obs);
                                               return false;
                                             }
                                             return true;
                                           }),
                            mCompositeObservers.end());
}

void SensorCore::activate(bool enable) {
  const SensorTypeIndex idx = mSensorData.type == BoschSensorType::ACCEL ? ACCEL_IDX : GYRO_IDX;

  if (enable) {
    mSensorThread->registerObserver(idx, shared_from_this());
  } else {
    mSensorThread->removeObserver(idx, shared_from_this());
  }

  activateByType(mSensorData.type, enable);
}

void SensorCore::activateByType(BoschSensorType type, bool enable) {
  bool isEnabled = false;
  bool enabledChanged = false;
  {
    std::lock_guard<std::mutex> lock(mRunMutex);
    mEnableState[type] = enable;
    mSensorValues.clear();

    if (mSamplingPeriods.find(type) == mSamplingPeriods.end()) {
      // use default sampling period if not set
      mSamplingPeriods[type] = mSensorData.maxDelayUs * 1000;
    }
    if (mReportLatency.find(type) == mReportLatency.end()) {
      // use default report latency if not set
      mReportLatency[type] = mSensorData.maxDelayUs * 1000;
    }

    isEnabled = std::any_of(mEnableState.begin(), mEnableState.end(), [](const auto& state) { return state.second; });
    enabledChanged = (isEnabled != mIsEnabled);
    mIsEnabled = isEnabled;
  }
  updateSamplingRate();

  if (enabledChanged) {
    enableSensor(isEnabled);
  }
}

void SensorCore::batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
  batchByType(mSensorData.type, samplingPeriodNs, maxReportLatencyNs);
}

void SensorCore::batchByType(BoschSensorType type, int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
  {
    std::lock_guard<std::mutex> lock(mRunMutex);
    mSamplingPeriods[type] = samplingPeriodNs;
    mReportLatency[type] = maxReportLatencyNs;
    mSensorValues.clear();
  }
  updateSamplingRate();
}

void SensorCore::flush() { mSensorThread->flush(); }

void SensorCore::updateSamplingRate() {
  bool updateNeeded = false;
  int64_t usedSamplingPeriod = mSensorData.maxDelayUs * 1000;
  int64_t usedReportLatency = std::numeric_limits<int64_t>::max();
  {
    std::lock_guard<std::mutex> lock(mRunMutex);
    for (const auto& samplingPeriod : mSamplingPeriods) {
      if (mEnableState[samplingPeriod.first] && (samplingPeriod.second <= usedSamplingPeriod)) {
        usedSamplingPeriod = samplingPeriod.second;
        updateNeeded = true;
      }
    }
    for (const auto& reportLatency : mReportLatency) {
      if (mEnableState[reportLatency.first] && (reportLatency.second <= usedReportLatency)) {
        usedReportLatency = reportLatency.second;
        updateNeeded = true;
      }
    }
  }

  if (updateNeeded) {
    setSamplingRate(usedSamplingPeriod, usedReportLatency);
  }
}

void SensorCore::updateValidTimestampFactor(int64_t odrNs) {
  std::vector<std::weak_ptr<ICompositeObserverCb>> observersCopy;
  {
    std::lock_guard<std::mutex> lock(mRunMutex);
    const int64_t samplingPeriodNs = mSamplingPeriods[mSensorData.type];
    const float factor = (samplingPeriodNs / odrNs) / (samplingPeriodNs / static_cast<float>(odrNs));
    mMinValidDiffNs = samplingPeriodNs * std::max(factor * VALID_TIMESTAMP_FACTOR, MIN_VALID_TIMESTAMP_FACTOR);
    ALOGD("SensorCore[%s]: Set check diff to %ld ns\n", mSensorData.sensorName.c_str(), mMinValidDiffNs);

    cleanAndCopyCompositeObservers(observersCopy);
  }

  for (const auto& obs : observersCopy) {
    if (auto observerPtr = obs.lock()) {
      observerPtr->updateValidTimestampFactor(odrNs);
    }
  }
}

bool SensorCore::readSensorTemperature(float* temperature) {
  if (mSensorData.temperatureSysfsRaw.empty()) return false;

  std::string data;
  bosch::hwctl::ReadHandler fileHandler(mDevice, mSensorData.temperatureSysfsRaw);

  if (0 == fileHandler.read(data)) {
    *temperature = (::atof(data.c_str()) + mSensorData.temperatureOffset) * mSensorData.temperatureScale;
    return true;
  } else {
    ALOGE("Failed to read sensor temperature");
    return false;
  }
}

void SensorCore::startThread(SensorCore* sensor) { sensor->run(); }

void SensorCore::run() {
  std::unique_lock<std::mutex> lock(mRunMutex);
  while (!mStopThread) {
    mWaitCV.wait(lock, [this] { return mStopThread || !mSensorValues.empty(); });

    if (mStopThread) break;

    std::vector<SensorValues> values;
    values.swap(mSensorValues);

    std::vector<std::weak_ptr<ISensorValues>> observersCopy;
    cleanAndCopyObservers(observersCopy);
    lock.unlock();

    for (const auto& obs : observersCopy) {
      if (auto observerPtr = obs.lock()) {
        observerPtr->update(values);
      }
    }

    lock.lock();
  }
}

void SensorCore::update(const std::vector<SensorValues>& values) {
  std::lock_guard<std::mutex> lock(mRunMutex);

  for (const auto& value : values) {
    if (value.flushRequest) {
      SensorValues metaData{};
      metaData.type = BoschSensorType::META_DATA;
      metaData.flushRequest = true;
      mSensorValues.push_back(metaData);
    } else {
      const int64_t timeDiff = value.timestamp - mLastTimestampNs;
      if (timeDiff >= mMinValidDiffNs) {
        mSensorValues.push_back(value);
        mLastTimestampNs = value.timestamp;
      }
    }
  }

  if (!mSensorValues.empty()) {
    mWaitCV.notify_all();
  }
}
