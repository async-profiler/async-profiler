/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef void* (*malloc_t)(size_t);
static malloc_t _orig_malloc = NULL;

__attribute__((visibility("default")))
void* malloc(size_t size) {
    if (_orig_malloc == NULL) {
        _orig_malloc = (malloc_t) dlsym(RTLD_NEXT, "malloc");
    }
    void* ptr = _orig_malloc(size);
    memset(ptr, 0xff, size);
    return ptr;
}
