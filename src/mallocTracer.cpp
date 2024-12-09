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
        MallocTracer::recordMalloc(ret, size);
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
#endif

static void* calloc_hook(size_t num, size_t size) {
    void* ret = _orig_calloc(num, size);
    if (likely(ret && num && size)) {
        MallocTracer::recordMalloc(ret, num * size);
    }
    return ret;
}

#ifdef __linux__
extern "C" WEAK DLLEXPORT void* calloc(size_t num, size_t size) {
    if (unlikely(!_orig_calloc)) {
        return NULL;
    }

    if (likely(MallocTracer::initialized())) {
        return calloc_hook(num, size);
    }
    return _orig_calloc(num, size);
}
#endif

static void* realloc_hook(void* addr, size_t size) {
    void* ret = _orig_realloc(addr, size);
    if (likely(ret && addr)) {
        MallocTracer::recordFree(addr);
    }

    if (likely(ret && size)) {
        MallocTracer::recordMalloc(ret, size);
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
#endif

static void free_hook(void* addr) {
    _orig_free(addr);
    if (addr) {
        MallocTracer::recordFree(addr);
    }
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
#endif

u64 MallocTracer::_interval;
volatile u64 MallocTracer::_allocated_bytes;

Mutex MallocTracer::_patch_lock;
int MallocTracer::_patched_libs = 0;
bool MallocTracer::_initialized = false;

// Only to be used from getOrigAddresses
void* safeCalloc(size_t num, size_t size) {
    void* ret = OS::safeAlloc(num * size);
    if (likely(ret)) {
        memset(ret, 0, num * size);
    }
    return ret;
}

__attribute__((constructor)) static void getOrigAddresses() {
    // Set malloc and calloc which may be called from libc during getOrigAddresses.
    _orig_malloc = OS::safeAlloc;
    _orig_calloc = safeCalloc;

    // Store these addresses, regardless of MallocTracer being enabled or not.
    _orig_malloc = ADDRESS_OF(malloc);
    _orig_calloc = ADDRESS_OF(calloc);
    _orig_realloc = ADDRESS_OF(realloc);
    _orig_free = ADDRESS_OF(free);
}

bool MallocTracer::initialize() {
    if (!__sync_bool_compare_and_swap(&_initialized, false, true)) {
        return false;
    }

    CodeCache* lib = Profiler::instance()->findLibraryByAddress((void*)MallocTracer::initialize);
    assert(lib);

    lib->mark(
        [](const char* s) -> bool {
            return strncmp(s, "_ZL11malloc_hook", 16) == 0
                || strncmp(s, "_ZL11calloc_hook", 16) == 0
                || strncmp(s, "_ZL12realloc_hook", 17) == 0
                || strncmp(s, "_ZL9free_hook", 13) == 0;
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

void MallocTracer::recordMalloc(void* address, size_t size) {
    if (updateCounter(_allocated_bytes, size, _interval)) {
        MallocEvent event;
        event._start_time = OS::nanotime();
        event._address = (uintptr_t)address;
        event._size = size;

        Profiler::instance()->recordSample(NULL, size, MALLOC_SAMPLE, &event);
    }
}

void MallocTracer::recordFree(void* address) {
    MallocEvent event;
    event._start_time = OS::nanotime();
    event._address = (uintptr_t)address;
    event._size = 0;

    Profiler::instance()->recordEventOnly(MALLOC_SAMPLE, &event);
}

Error MallocTracer::check(Arguments& args) {
    if (!OS::isLinux()) {
        return Error("nativemem option is only supported on linux.");
    } else {
        return Error::OK;
    }
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
