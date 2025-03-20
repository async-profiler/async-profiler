/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "threadLocalData.h"

static pthread_key_t init_profiler_data_key() {
    pthread_key_t profiler_data_key;

    if (pthread_key_create(&profiler_data_key, free) != 0) {
        return -1;
    }

    return profiler_data_key;
}

// A key that points to a malloc'd asprof_thread_local_data
pthread_key_t ThreadLocalData::_profiler_data_key = init_profiler_data_key();

// Initialize the *thread-local* profiler data key. The global data key (_profiler_data_key)
// should be initialized beforehand.
asprof_thread_local_data* ThreadLocalData::initThreadLocalData(pthread_key_t profiler_data_key) {
    // Initialize. Since this is a thread-local, it is not racy.
    asprof_thread_local_data* val = (asprof_thread_local_data*) malloc(sizeof(asprof_thread_local_data));
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
