/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ASPROF_THREAD_LOCAL_H
#define _ASPROF_THREAD_LOCAL_H

#include "asprof.h"
#include "mutex.h"
#include "arguments.h"
#include <pthread.h>
#include <atomic>

class ThreadLocalData {
public:
    // Initialize the thread-local data. Until this function is called, incrementSampleCounter is a no-op and
    // getThreadLocalData will return NULL.
    //
    // This function should not be called concurrently with itself, but is safe to call concurrently
    // with getThreadLocalData. This is important since customer applications might call getThreadLocalData
    // without synchronization.
    static Error initThreadLocalData();

    // increment the thread-local sample counter. See the `asprof_thread_local_data` docs.
    static void incrementSampleCounter(void)  {
        pthread_key_t profiler_data_key = _profiler_data_key.load(std::memory_order_acquire);
        if (profiler_data_key == -1) return;
    
        asprof_thread_local_data *data = (asprof_thread_local_data *)pthread_getspecific(profiler_data_key);
        if (data != NULL) {
            data->sample_counter++;
        }
    }

    // Get the `asprof_thread_local_data`. See the `asprof_get_thread_local_data` docs.
    static asprof_thread_local_data *getThreadLocalData(void)  {
        pthread_key_t profiler_data_key = _profiler_data_key.load(std::memory_order_acquire);
        if (profiler_data_key == -1) {
            return NULL;
        }
    
        asprof_thread_local_data *val = (asprof_thread_local_data*) pthread_getspecific(profiler_data_key);
        if (val == NULL) {
            return initPerThreadThreadLocalData(profiler_data_key);
        }
    
        return val;
    }
private:
    static asprof_thread_local_data *initPerThreadThreadLocalData(pthread_key_t profiler_data_key);
    static std::atomic<pthread_key_t> _profiler_data_key;
};

#endif // _ASPROF_THREAD_LOCAL_H
