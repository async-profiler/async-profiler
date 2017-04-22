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

#if defined(__arm__) || defined(__thumb__)
#define spinPause() __asm__ __volatile__ ("yield"   : : : )
#else
#define spinPause()  asm volatile("pause")
#endif

// Cannot use regular mutexes inside signal handler
class SpinLock {
  private:
    int _lock;

  public:
    SpinLock() : _lock(0) {
    }

    void reset() {
        _lock = 0;
    }

    bool tryLock() {
        return __sync_lock_test_and_set(&_lock, 1) == 0;
    }

    void spinLock() {
        while (!tryLock()) {
            spinPause();
        }
    }

    void unlock() {
        __sync_lock_release(&_lock);
    }
};

#endif // _SPINLOCK_H
