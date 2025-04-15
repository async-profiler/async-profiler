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
    // `asprof_get_thread_local_data` is called on a given thread. Further calls to
    // `asprof_get_thread_local_data` on a given thread will of course not reset the counter.
    volatile uint64_t sample_counter;
} asprof_thread_local_data;

// This API is UNSTABLE and might change or be removed in the next version of async-profiler.
//
// Gets a pointer to asprof's thread-local data structure, see `asprof_thread_local_data`'s
// documentation for the details of each field. This function might lazily initialize that
// structure.
//
// This function can return NULL either if the profiler is not yet initializer, or in
// case of an allocation failure.
//
// This function is *not* async-signal-safe. However, it is safe to call concurrently
// with async-profiler operations, including initialization.
DLLEXPORT asprof_thread_local_data* asprof_get_thread_local_data(void);
typedef asprof_thread_local_data* (*asprof_get_thread_local_data_t)(void);


typedef int asprof_jfr_event_key;

// This API is UNSTABLE and might change or be removed in the next version of async-profiler.
//
// Return a asprof_jfr_event_key identifier for a user-defined JFR key.
// That identifier can then be used in `asprof_emit_jfr_event`
//
// The name is required to be valid (since it's a C string, NUL-free) UTF-8.
//
// Returns -1 on failure.
DLLEXPORT asprof_jfr_event_key asprof_register_jfr_event(const char* name);
typedef asprof_jfr_event_key (*asprof_register_jfr_event_t)(const char* name);


#define ASPROF_MAX_JFR_EVENT_LENGTH 2048

// This API is UNSTABLE and might change or be removed in the next version of async-profiler.
//
// Emits a custom, user-defined JFR event. The key should be created via `asprof_register_jfr_event`.
// The data can be arbitrary binary data, with size <= ASPROF_MAX_JFR_EVENT_LENGTH.
//
// User-defined events are included in the JFR under a `profiler.UserEvent` event type. That type will contain
// (at least) the following fields:
// 1. `startTime` [Long] - the emitted event's time in ticks.
// 2. `eventThread` [java.lang.Thread] - the thread that emitted the events.
// 3. `type` [profiler.types.UserEventType] - the event's type,
//    where `profiler.types.UserEventType` is an indexed string from the JFR constant pool.
// 4. `data` [String] - the event data. This is the Latin-1 encoded version of the inputted data.
//    The Latin-1 encoding is used as a way to stuff the arbitrary byte input into something
//    that JFR supports (JFR technically supports byte arrays, but `jfr print` doesn't).
//
// Returns an error code or NULL on success.
DLLEXPORT asprof_error_t asprof_emit_jfr_event(asprof_jfr_event_key type, const uint8_t* data, size_t len);
typedef asprof_error_t (*asprof_emit_jfr_event_t)(asprof_jfr_event_key type, const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _ASPROF_H
