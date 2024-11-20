/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asprof.h"
#include "assert.h"
#include "codeCache.h"
#include "mallocTracer.h"
#include "os.h"
#include "profiler.h"
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

static void* malloc_hook(size_t size) {
    void* ret = _orig_calloc(1, size);
    if (likely(ret && size)) {
        MallocTracer::recordMalloc(ret, size, OS::nanotime());
    }
    return ret;
}

#ifdef __linux__
extern "C" WEAK DLLEXPORT void* malloc(size_t size) {
    if (unlikely(!_orig_malloc)) {
        return NULL;
    }

    if (likely(MallocTracer::initialized())) {
        return malloc_hook(size);
    }
    return _orig_malloc(size);
}
#endif // __linux__

static void* calloc_hook(size_t num, size_t size) {
    void* ret = _orig_calloc(num, size);
    if (likely(ret && num && size)) {
        MallocTracer::recordMalloc(ret, num * size, OS::nanotime());
    }
    return ret;
}

#ifdef __linux__
extern "C" WEAK DLLEXPORT void* calloc(size_t num, size_t size) {
    if (unlikely(!_orig_calloc)) {
        // In some libc versions, dlsym may call calloc on error case in the ADDRESS_OF macro.
        return NULL;
    }

    if (likely(MallocTracer::initialized())) {
        return calloc_hook(num, size);
    }
    return _orig_calloc(num, size);
}
#endif // __linux__

static void* realloc_hook(void* addr, size_t size) {
    void* ret = _orig_realloc(addr, size);
    if (likely(ret && addr)) {
        MallocTracer::recordFree(addr, OS::nanotime());
    }

    if (likely(ret && size)) {
        MallocTracer::recordMalloc(ret, size, OS::nanotime());
    }
    return ret;
}

#ifdef __linux__
extern "C" WEAK DLLEXPORT void* realloc(void* addr, size_t size) {
    if (unlikely(!_orig_realloc)) {
        return NULL;
    }

    if (likely(MallocTracer::initialized())) {
        return realloc_hook(addr, size);
    }
    return _orig_realloc(addr, size);
}
#endif // __linux__

static void free_hook(void* addr) {
    _orig_free(addr);
    MallocTracer::recordFree(addr, OS::nanotime());
}

#ifdef __linux__
extern "C" WEAK DLLEXPORT void free(void* addr) {
    if (unlikely(!_orig_free)) {
        return;
    }

    if (likely(MallocTracer::initialized())) {
        return free_hook(addr);
    }
    return _orig_free(addr);
}
#endif // __linux__

u64 MallocTracer::_interval;
volatile u64 MallocTracer::_allocated_bytes;

Mutex MallocTracer::_patch_lock;
int MallocTracer::_patched_libs = 0;
bool MallocTracer::_initialized = false;

__attribute__((constructor)) static void getOrigAddresses() {
    // Store these addresses, regardless of MallocTracer being enabled or not.
    _orig_calloc = ADDRESS_OF(calloc);
    _orig_free = ADDRESS_OF(free);
    _orig_malloc = ADDRESS_OF(malloc);
    _orig_realloc = ADDRESS_OF(realloc);
}

bool MallocTracer::initialize() {
    if (!__sync_bool_compare_and_swap(&_initialized, false, true)) {
        return false;
    }

    CodeCache* lib = Profiler::instance()->findLibraryByName("libasyncProfiler");
    assert(lib);

    lib->mark(
        [](const char* s) -> bool {
            return strncmp(s, "_ZL11malloc_hook", 16) == 0 || strncmp(s, "_ZL11calloc_hook", 16) == 0 || strncmp(s, "_ZL12realloc_hook", 16) == 0 || strncmp(s, " _ZL9free_hook", 13) == 0;
        },
        MARK_ASYNC_PROFILER);

    return installHooks();
}

bool MallocTracer::patchLibs(bool install) {
    if (!initialized()) {
        return false;
    }

    MutexLocker ml(_patch_lock);
    if (!install) {
        assert(_orig_malloc);
        assert(_orig_calloc);
        assert(_orig_realloc);
        assert(_orig_free);

        _patched_libs = 0;
    }

    CodeCacheArray* native_libs = Profiler::instance()->nativeLibs();
    int native_lib_count = native_libs->count();

    while (_patched_libs < native_lib_count) {
        CodeCache* cc = (*native_libs)[_patched_libs++];

        cc->patchImport(im_malloc, (void*)(install ? malloc_hook : _orig_malloc));
        cc->patchImport(im_calloc, (void*)(install ? calloc_hook : _orig_calloc));
        cc->patchImport(im_realloc, (void*)(install ? realloc_hook : _orig_realloc));
        cc->patchImport(im_free, (void*)(install ? free_hook : _orig_free));
    }

    if (!install) {
        _patched_libs = 0;
    }

    return true;
}

void MallocTracer::recordMalloc(void* address, size_t size, u64 time) {
    if (updateCounter(_allocated_bytes, size, _interval)) {
        MallocEvent event;
        event._start_time = time;
        event._address = (uintptr_t)address;
        event._size = size;

        Profiler::instance()->recordSample(NULL, size, MALLOC_SAMPLE, &event);
    }
}

void MallocTracer::recordFree(void* address, u64 time) {
    MallocEvent event;
    event._start_time = time;
    event._address = (uintptr_t)address;
    event._size = 0;

    Profiler::instance()->recordSample(NULL, 0, MALLOC_SAMPLE, &event);
}

Error MallocTracer::check(Arguments& args) {
#ifndef __linux__
    return Error("nativemem option is only supported on linux.");
#endif
    return Error::OK;
}

Error MallocTracer::start(Arguments& args) {
    Error error = check(args);
    if (error) {
        return error;
    }

    _interval = args._nativemem > 0 ? args._nativemem : 0;
    _allocated_bytes = 0;

    if (!initialize() && initialized()) {
        // Restart.
        installHooks();
    }

    return Error::OK;
}

void MallocTracer::stop() {
    patchLibs(false);
}
