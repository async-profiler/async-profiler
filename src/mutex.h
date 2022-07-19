/*
 * Copyright 2018 Andrei Pangin
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

#ifndef _MUTEX_H
#define _MUTEX_H

#include <pthread.h>
#include "arch.h"


class Mutex {
  protected:
    pthread_mutex_t _mutex;

  public:
    Mutex();

    void lock();
    void unlock();
};

class WaitableMutex : public Mutex {
  protected:
    pthread_cond_t _cond;

  public:
    WaitableMutex();

    bool waitUntil(u64 wall_time);
    void notify();
};

class MutexLocker {
  private:
    Mutex* _mutex;

  public:
    MutexLocker(Mutex& mutex) : _mutex(&mutex) {
        _mutex->lock();
    }

    ~MutexLocker() {
        _mutex->unlock();
    }
};

#endif // _MUTEX_H
