/*
 * Copyright (C) 2017 The Android Open Source Project
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
#include "DirectChannel.h"

#include <cutils/ashmem.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <atomic>

namespace android {

LockfreeBuffer::LockfreeBuffer(void* buf, size_t size)
  : mData((sensors_event_t*)buf), mSize(size / sizeof(sensors_event_t)), mWritePos(0), mCounter(1) {
  memset(mData, 0, size);
}

LockfreeBuffer::~LockfreeBuffer() { memset(mData, 0, mSize * sizeof(sensors_event_t)); }

void LockfreeBuffer::write(const sensors_event_t* ev, size_t size) {
  if (!mSize) {
    return;
  }

  while (size--) {
    // part before reserved0 field
    memcpy(&mData[mWritePos], ev, offsetof(sensors_event_t, reserved0));
    // part after reserved0 field
    memcpy(reinterpret_cast<char*>(&mData[mWritePos]) + offsetof(sensors_event_t, timestamp),
           reinterpret_cast<const char*>(ev) + offsetof(sensors_event_t, timestamp),
           sizeof(sensors_event_t) - offsetof(sensors_event_t, timestamp));
    // barrier before writing the atomic counter
    std::atomic_thread_fence(std::memory_order_release);
    mData[mWritePos].reserved0 = mCounter++;
    // barrier after writing the atomic counter
    std::atomic_thread_fence(std::memory_order_release);
    ++ev;

    if (++mWritePos >= mSize) {
      mWritePos = 0;
    }
  }
}

bool DirectChannelBase::isValid() { return mBuffer != nullptr; }

int DirectChannelBase::getError() { return mError; }

void DirectChannelBase::write(const sensors_event_t* ev) {
  if (isValid()) {
    mBuffer->write(ev, 1);
  }
}

AshmemDirectChannel::AshmemDirectChannel(const struct sensors_direct_mem_t* mem) : mAshmemFd(0) {
  mAshmemFd = mem->handle->data[0];

  if (!::ashmem_valid(mAshmemFd)) {
    mError = BAD_VALUE;
    return;
  }

  if ((size_t)::ashmem_get_size_region(mAshmemFd) != mem->size) {
    mError = BAD_VALUE;
    return;
  }

  mSize = mem->size;

  mBase = ::mmap(NULL, mSize, PROT_WRITE, MAP_SHARED, mAshmemFd, 0);
  if (mBase == nullptr) {
    mError = NO_MEMORY;
    return;
  }

  mBuffer = std::unique_ptr<LockfreeBuffer>(new LockfreeBuffer(mBase, mSize));
  if (!mBuffer) {
    mError = NO_MEMORY;
  }
}

AshmemDirectChannel::~AshmemDirectChannel() {
  if (mBase) {
    mBuffer = nullptr;
    ::munmap(mBase, mSize);
    mBase = nullptr;
  }
  ::close(mAshmemFd);
}

}  // namespace android