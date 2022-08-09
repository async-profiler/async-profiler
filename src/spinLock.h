/*
 * Copyright 2017 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include "arch.h"


// Cannot use regular mutexes inside signal handler.
// This lock is based on CAS busy loop. GCC atomic builtins imply full barrier.
class SpinLock {
  private:
    //  0 - unlocked
    //  1 - exclusive lock
    // <0 - shared lock
    volatile int _lock;

  public:
    constexpr SpinLock(int initial_state = 0) : _lock(initial_state) {
    }

    void reset() {
        _lock = 0;
    }

    bool tryLock() {
        return __sync_bool_compare_and_swap(&_lock, 0, 1);
    }

    void lock() {
        while (!tryLock()) {
            spinPause();
        }
    }

    void unlock() {
        __sync_fetch_and_sub(&_lock, 1);
    }

    bool tryLockShared() {
        int value;
        while ((value = _lock) <= 0) {
            if (__sync_bool_compare_and_swap(&_lock, value, value - 1)) {
                return true;
            }
        }
        return false;
    }

    void lockShared() {
        int value;
        while ((value = _lock) > 0 || !__sync_bool_compare_and_swap(&_lock, value, value - 1)) {
            spinPause();
        }
    }

    void unlockShared() {
        __sync_fetch_and_add(&_lock, 1);
    }
};

#endif // _SPINLOCK_H
