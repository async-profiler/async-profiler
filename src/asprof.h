/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ASPROF_H
#define _ASPROF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __clang__
#  define DLLEXPORT __attribute__((visibility("default")))
#else
#  define DLLEXPORT __attribute__((visibility("default"),externally_visible))
#endif

#define WEAK __attribute__((weak))

#ifdef __cplusplus
extern "C" {
#endif


typedef const char* asprof_error_t;
typedef void (*asprof_writer_t)(const char* buf, size_t size);

// Should be called once prior to any other API functions
DLLEXPORT void asprof_init();
typedef void (*asprof_init_t)();

// Returns an error message for the given error code or NULL if there is no error
DLLEXPORT const char* asprof_error_str(asprof_error_t err);
typedef const char* (*asprof_error_str_t)(asprof_error_t err);

// Executes async-profiler command using output_callback as an optional sink
// for the profiler output. Returning an error code or NULL on success.
DLLEXPORT asprof_error_t asprof_execute(const char* command, asprof_writer_t output_callback);
typedef asprof_error_t (*asprof_execute_t)(const char* command, asprof_writer_t output_callback);

// This API is UNSTABLE and might change or be removed in the next version of async-profiler.
typedef struct {
    // A thread-local sample counter, which increments (not necessarily by 1) every time a
    // stack profiling sample is taken using a profiling signal.
    //
    // The counter might be initialized lazily, only starting counting from 0 the first time
    // `asprof_unstable_get_thread_local_data` is called on a given thread. Further calls to
    // `asprof_unstable_get_thread_local_data` on a given thread will of course not reset the counter.
    volatile uint64_t sample_counter;
} asprof_unstable_thread_local_data;

// This API is UNSTABLE and might change or be removed in the next version of async-profiler.
//
// Gets a pointer to asprof's thread-local data structure, see `asprof_unstable_thread_local_data`'s
// documentation for the details of each field. This function might lazily initialize that
// structure.
//
// This function can return NULL in case of an allocation failure.
//
// This function is *not* async-signal safe.
DLLEXPORT asprof_unstable_thread_local_data *asprof_unstable_get_thread_local_data(void);
typedef asprof_unstable_thread_local_data *(*asprof_unstable_get_thread_local_data_t)(void);

#ifdef __cplusplus
}
#endif

#endif // _ASPROF_H
