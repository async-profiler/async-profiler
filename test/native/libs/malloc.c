/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#ifdef __linux__

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

#else

typedef void* (*malloc_t)(size_t);
static malloc_t _orig_malloc = malloc;

__attribute__((visibility("default")))
void* mac_malloc(size_t size) {
    void* ptr = _orig_malloc(size);
    memset(ptr, 0xff, size);
    return ptr;
}

__attribute__((used)) static struct {
    const void* replacement;
    const void* original;
} interposers[] __attribute__((section("__DATA,__interpose"))) = {
    { (const void*)mac_malloc, (const void*)malloc }
};

#endif
