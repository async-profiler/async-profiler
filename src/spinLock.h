/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
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
    SpinLock(int initial_state = 0) : _lock(initial_state) {
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
