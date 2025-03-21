/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ASPROF_THREAD_LOCAL_H
#define _ASPROF_THREAD_LOCAL_H

#include "asprof.h"
#include <pthread.h>

class ThreadLocalData {
  public:
    // This function *is* async-signal safe, and therefore will not initialize the thread-local
    // storage by itself, but will rather do nothing if it's not initialized already. This works
    // fine with the `sample_counter` API since only changes in `sample_counter` matter.
    static asprof_thread_local_data* getIfPresent()  {
        if (_profiler_data_key == -1) {
            return NULL;
        }

        return (asprof_thread_local_data*) pthread_getspecific(_profiler_data_key);
    }

    // Get the `asprof_thread_local_data`. See the `asprof_get_thread_local_data` docs.
    static asprof_thread_local_data* get()  {
        if (_profiler_data_key == -1) {
            return NULL;
        }

        asprof_thread_local_data* val = (asprof_thread_local_data*) pthread_getspecific(_profiler_data_key);
        if (val == NULL) {
            return initThreadLocalData(_profiler_data_key);
        }

        return val;
    }

  private:
    static asprof_thread_local_data* initThreadLocalData(pthread_key_t profiler_data_key);
    static pthread_key_t _profiler_data_key;
};

#endif // _ASPROF_THREAD_LOCAL_H
