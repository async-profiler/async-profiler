/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asprof.h"
#include "asprofPrivate.h"
#include "hooks.h"
#include "profiler.h"
#include <mutex>
#include <atomic>

// A key that points to a malloc'd uint64_t* sample counter, which
// increments by 1 every time a signal is received to allow the
// application to detect samples.
//
// The per-thread counter is lazily initialized in `asprof_unstable_get_thread_local_data`,
// and is NULL until `asprof_unstable_get_thread_local_data` is called. This matches the API
// since the sample counter only needs to start running the first time
// `asprof_unstable_get_thread_local_data` is called, and allows us to avoid
// calling `malloc` in weird places.
//
// Using std::atomic instead of "native" std::call_once to ensure signal safety.
static std::atomic<pthread_key_t> _profiler_data_key {(pthread_key_t)-1};

// Mutex to make sure initialization is not racy.
static std::mutex _init_mutex;

static asprof_error_t asprof_error(const char* msg) {
    return (asprof_error_t)msg;
}


DLLEXPORT void asprof_init() {
    Hooks::init(true);
}

DLLEXPORT const char* asprof_error_str(asprof_error_t err) {
    return err;
}

DLLEXPORT asprof_error_t asprof_execute(const char* command, asprof_writer_t output_callback) {
    Arguments args;
    Error error = args.parse(command);
    if (error) {
        return asprof_error(error.message());
    }

    Log::open(args);

    if (!args.hasOutputFile()) {
        CallbackWriter out(output_callback);
        error = Profiler::instance()->runInternal(args, out);
        if (!error) {
            return NULL;
        }
    } else {
        FileWriter out(args.file());
        if (!out.is_open()) {
            return asprof_error("Could not open output file");
        }
        error = Profiler::instance()->runInternal(args, out);
        if (!error) {
            return NULL;
        }
    }

    return asprof_error(error.message());
}

// Initialize the *global* profiler data key.
static pthread_key_t asprof_init_global_profiler_data_key() {
    const std::lock_guard<std::mutex> lock(_init_mutex);

    // relaxed is OK here since the lock synchronizes the stores
    pthread_key_t profiler_data_key = _profiler_data_key.load(std::memory_order_relaxed);
    if (profiler_data_key != -1) {
        return profiler_data_key; // already initialized
    }

    if (pthread_key_create(&profiler_data_key, free) != 0) {
        return -1;
    }
    _profiler_data_key.store(profiler_data_key, std::memory_order_release);
    return profiler_data_key;
}

// Initialize the *thread-local* profiler data key. The global data key (_profiler_data_key)
// should be initialized beforehand.
static asprof_unstable_thread_local_data *asprof_init_profiler_thread_local_data(pthread_key_t profiler_data_key) {
    // Initialize. Since this is a thread-local, it is not racy.
    asprof_unstable_thread_local_data *val = (asprof_unstable_thread_local_data*) malloc(sizeof(asprof_unstable_thread_local_data));
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

DLLEXPORT asprof_unstable_thread_local_data *asprof_unstable_get_thread_local_data(void) {
    pthread_key_t profiler_data_key = _profiler_data_key.load(std::memory_order_acquire);
    if (profiler_data_key == -1) {
        profiler_data_key = asprof_init_global_profiler_data_key();
        if (profiler_data_key == -1) {
            // unable to initialize
            return NULL;
        }
    }

    asprof_unstable_thread_local_data *val = (asprof_unstable_thread_local_data*) pthread_getspecific(profiler_data_key);
    if (val == NULL) {
        return asprof_init_profiler_thread_local_data(profiler_data_key);
    }

    return val;
}

void asprofIncrementThreadLocalSampleCounter() {
    // over-paranoia - access the sample key once to avoid weird signal races
    pthread_key_t profiler_data_key = _profiler_data_key.load(std::memory_order_acquire);
    if (profiler_data_key == -1) return;

    asprof_unstable_thread_local_data *data = (asprof_unstable_thread_local_data *)pthread_getspecific(profiler_data_key);
    if (data != NULL) {
        data->sample_counter++;
    }
}
