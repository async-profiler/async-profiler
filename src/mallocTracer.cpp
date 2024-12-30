/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asprof.h"
#include "assert.h"
#include "codeCache.h"
#include "mallocTracer.h"
#include "profiler.h"
#include "tsc.h"
#include <dlfcn.h>
#include <string.h>

#define ADDRESS_OF(sym) ({               \
    void* addr = dlsym(RTLD_NEXT, #sym); \
    addr != NULL ? (sym##_t)addr : sym;  \
})

typedef void* (*malloc_t)(size_t);
static malloc_t _orig_malloc = NULL;

typedef void* (*calloc_t)(size_t, size_t);
static calloc_t _orig_calloc = NULL;

typedef void* (*realloc_t)(void*, size_t);
static realloc_t _orig_realloc = NULL;

typedef void (*free_t)(void*);
static free_t _orig_free = NULL;

__attribute__((constructor)) static void getOrigAddresses() {
    // Store these addresses, regardless of MallocTracer being enabled or not.
    _orig_malloc = ADDRESS_OF(malloc);
    _orig_calloc = ADDRESS_OF(calloc);
    _orig_realloc = ADDRESS_OF(realloc);
    _orig_free = ADDRESS_OF(free);
}

extern "C" void* malloc_hook(size_t size) {
    void* ret = _orig_malloc(size);
    if (MallocTracer::running() && ret && size) {
        MallocTracer::recordMalloc(ret, size);
    }
    return ret;
}

extern "C" void* calloc_hook(size_t num, size_t size) {
    void* ret = _orig_calloc(num, size);
    if (MallocTracer::running() && ret && num && size) {
        MallocTracer::recordMalloc(ret, num * size);
    }
    return ret;
}

extern "C" void* realloc_hook(void* addr, size_t size) {
    void* ret = _orig_realloc(addr, size);
    if (MallocTracer::running() && ret) {
        if (addr) {
            MallocTracer::recordFree(addr);
        }
        if (size) {
            MallocTracer::recordMalloc(ret, size);
        }
    }
    return ret;
}

extern "C" void free_hook(void* addr) {
    _orig_free(addr);
    if (MallocTracer::running() && addr) {
        MallocTracer::recordFree(addr);
    }
}


u64 MallocTracer::_interval;
volatile u64 MallocTracer::_allocated_bytes;

Mutex MallocTracer::_patch_lock;
int MallocTracer::_patched_libs = 0;
bool MallocTracer::_initialized = false;
volatile bool MallocTracer::_running = false;

void MallocTracer::initialize() {
    CodeCache* lib = Profiler::instance()->findLibraryByAddress((void*)MallocTracer::initialize);
    assert(lib);

    lib->mark(
        [](const char* s) -> bool {
            return strcmp(s, "malloc_hook") == 0
                || strcmp(s, "calloc_hook") == 0
                || strcmp(s, "realloc_hook") == 0
                || strcmp(s, "free_hook") == 0;
        },
        MARK_ASYNC_PROFILER);
}

void MallocTracer::patchLibraries() {
    MutexLocker ml(_patch_lock);

    CodeCacheArray* native_libs = Profiler::instance()->nativeLibs();
    int native_lib_count = native_libs->count();

    while (_patched_libs < native_lib_count) {
        CodeCache* cc = (*native_libs)[_patched_libs++];

        cc->patchImport(im_malloc, (void*)malloc_hook);
        cc->patchImport(im_calloc, (void*)calloc_hook);
        cc->patchImport(im_realloc, (void*)realloc_hook);
        cc->patchImport(im_free, (void*)free_hook);
    }
}

void MallocTracer::recordMalloc(void* address, size_t size) {
    if (updateCounter(_allocated_bytes, size, _interval)) {
        MallocEvent event;
        event._start_time = TSC::ticks();
        event._address = (uintptr_t)address;
        event._size = size;

        Profiler::instance()->recordSample(NULL, size, MALLOC_SAMPLE, &event);
    }
}

void MallocTracer::recordFree(void* address) {
    MallocEvent event;
    event._start_time = TSC::ticks();
    event._address = (uintptr_t)address;
    event._size = 0;

    Profiler::instance()->recordEventOnly(MALLOC_SAMPLE, &event);
}

Error MallocTracer::start(Arguments& args) {
    _interval = args._nativemem > 0 ? args._nativemem : 0;
    _allocated_bytes = 0;

    if (!_initialized) {
        initialize();
        _initialized = true;
    }

    _running = true;
    patchLibraries();

    return Error::OK;
}

void MallocTracer::stop() {
    // Ideally, we should reset original malloc entries, but it's not currently safe
    // in the view of library unloading. Consider using dl_iterate_phdr.
    _running = false;
}
