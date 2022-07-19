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

#include "mutex.h"


Mutex::Mutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_mutex, &attr);
}

void Mutex::lock() {
    pthread_mutex_lock(&_mutex);
}

void Mutex::unlock() {
    pthread_mutex_unlock(&_mutex);
}

WaitableMutex::WaitableMutex() : Mutex() {
    pthread_cond_init(&_cond, NULL);
}

bool WaitableMutex::waitUntil(u64 wall_time) {
    struct timespec ts = {(time_t)(wall_time / 1000000), (long)(wall_time % 1000000) * 1000};
    return pthread_cond_timedwait(&_cond, &_mutex, &ts) != 0;
}

void WaitableMutex::notify() {
    pthread_cond_signal(&_cond);
}
