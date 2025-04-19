/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

__attribute__((visibility("default")))
void* call_malloc(size_t size) {
    return malloc(size);
}
