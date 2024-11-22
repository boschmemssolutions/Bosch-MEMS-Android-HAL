/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef DIRECTCHANNEL_H_
#define DIRECTCHANNEL_H_

#include <cutils/native_handle.h>
#include <hardware/sensors.h>
#include <utils/threads.h>

#include <map>
#include <memory>
#include <vector>

namespace android {

struct LockfreeBuffer {
  LockfreeBuffer(void* buf, size_t size);
  ~LockfreeBuffer();

  // support single writer
  void write(const sensors_event_t* ev, size_t size);

private:
  sensors_event_t* mData;
  size_t mSize;
  size_t mWritePos;
  int32_t mCounter;

  LockfreeBuffer(const LockfreeBuffer&);
  LockfreeBuffer& operator=(const LockfreeBuffer&); /* NOLINT */
};

class DirectChannelBase {
public:
  DirectChannelBase() = default;
  virtual ~DirectChannelBase() = default;

  bool isValid();
  int getError();
  void write(const sensors_event_t* ev);

  std::vector<int32_t> sensorHandles;
  std::map<int32_t, int64_t> rateNs;
  std::map<int32_t, int32_t> sampleCount;

protected:
  int mError = NO_INIT;
  std::unique_ptr<LockfreeBuffer> mBuffer;

  size_t mSize = 0;
  void* mBase = nullptr;
};

class AshmemDirectChannel : public DirectChannelBase {
public:
  AshmemDirectChannel(const struct sensors_direct_mem_t* mem);
  ~AshmemDirectChannel() override;

private:
  int mAshmemFd;
};

}  // namespace android

#endif  // DIRECTCHANNEL_H_