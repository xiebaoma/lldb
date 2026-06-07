// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LLDB_UTIL_MUTEXLOCK_H_
#define STORAGE_LLDB_UTIL_MUTEXLOCK_H_

#include "port/port.h"

namespace lldb {

class MutexLock {
 public:
  explicit MutexLock(port::Mutex* mu) : mu_(mu) {
    this->mu_->Lock();
  }
  ~MutexLock() { this->mu_->Unlock(); }

  MutexLock(const MutexLock&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;

 private:
  port::Mutex* const mu_;
};

}  // namespace lldb

#endif  // STORAGE_LLDB_UTIL_MUTEXLOCK_H_
