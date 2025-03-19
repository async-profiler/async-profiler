/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "threadLocalData.h"

// A key that points to a malloc'd uint64_t* sample counter, which
// increments by 1 every time a signal is received to allow the
// application to detect samples.
//
// The per-thread counter is lazily initialized in `asprof_get_thread_local_data`,
// and is NULL until `asprof_get_thread_local_data` is called. This matches the API
// since the sample counter only needs to start running the first time
// `asprof_get_thread_local_data` is called, and allows us to avoid
// calling `malloc` in weird places.
//
// Using std::atomic instead of "native" std::call_once to ensure signal safety.
std::atomic<pthread_key_t> ThreadLocalData::_profiler_data_key {(pthread_key_t)-1};

Error ThreadLocalData::initThreadLocalData() {
    // relaxed is OK here since this is only to prevent re-initialization
    pthread_key_t profiler_data_key = _profiler_data_key.load(std::memory_order_relaxed);
    if (profiler_data_key != -1) {
        return Error::OK; // already initialized
    }

    if (pthread_key_create(&profiler_data_key, free) != 0) {
        return Error("unable to initialize thread-local data");
    }
    _profiler_data_key.store(profiler_data_key, std::memory_order_release);
    return Error::OK;
}

// Initialize the *thread-local* profiler data key. The global data key (_profiler_data_key)
// should be initialized beforehand.
asprof_thread_local_data *ThreadLocalData::initPerThreadThreadLocalData(pthread_key_t profiler_data_key) {
    // Initialize. Since this is a thread-local, it is not racy.
    asprof_thread_local_data *val = (asprof_thread_local_data*) malloc(sizeof(asprof_thread_local_data));
    if (val == NULL) {
        // would rather not insert random aborts into code. This
        // will make the code try again next time, which is fine.
        return NULL;
    }
    val->sample_counter = 0;
    if (pthread_setspecific(profiler_data_key, (void*)val) < 0) {
        free((void*)val);
        return NULL;
    }
    return val;
}

