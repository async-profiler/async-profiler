/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
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
