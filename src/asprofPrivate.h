/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ASPROF_PRIVATE_H
#define _ASPROF_PRIVATE_H

#include "asprof.h"

// Private functions for the native API.

// Increment the thread-local sample counter, if it's already initialized.
// If the counter is not already initialized, this function is a no-op.
//
// This function is signal-safe and safe to call racily with initialization.
void asprofIncrementThreadLocalSampleCounter(void);

#endif
