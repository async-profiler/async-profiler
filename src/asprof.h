/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ASPROF_H
#define _ASPROF_H

#include <stddef.h>

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


#ifdef __cplusplus
}
#endif

#endif // _ASPROF_H
