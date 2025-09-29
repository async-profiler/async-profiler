/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "asprof.h"
#include "assert.h"
#include "codeCache.h"
#include "mallocTracer.h"
#include "os.h"
#include "profiler.h"
#include "symbols.h"
#include "tsc.h"

#ifdef __clang__
#  define NO_OPTIMIZE __attribute__((optnone))
#else
#  define NO_OPTIMIZE __attribute__((optimize("-fno-omit-frame-pointer,-fno-optimize-sibling-calls")))
#endif

#define SAVE_IMPORT(FUNC) \
    _orig_##FUNC = (decltype(_orig_##FUNC))*lib->findImport(im_##FUNC)

static void* (*_orig_malloc)(size_t);
static void (*_orig_free)(void*);
static void* (*_orig_calloc)(size_t, size_t);
static void* (*_orig_realloc)(void*, size_t);
static int (*_orig_posix_memalign)(void**, size_t, size_t);
static void* (*_orig_aligned_alloc)(size_t, size_t);

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

// Make sure this is not optimized away (function-scoped -fno-optimize-sibling-calls)
extern "C" NO_OPTIMIZE
void* calloc_hook_dummy(size_t num, size_t size) {
    return _orig_calloc(num, size);
}

extern "C" void* realloc_hook(void* addr, size_t size) {
    void* ret = _orig_realloc(addr, size);
    if (MallocTracer::running() && ret) {
        if (addr && !MallocTracer::nofree()) {
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
    if (MallocTracer::running() && !MallocTracer::nofree() && addr) {
        MallocTracer::recordFree(addr);
    }
}

extern "C" int posix_memalign_hook(void** memptr, size_t alignment, size_t size) {
    int ret = _orig_posix_memalign(memptr, alignment, size);
    if (MallocTracer::running() && ret == 0 && memptr && *memptr && size) {
        MallocTracer::recordMalloc(*memptr, size);
    }
    return ret;
}

// Make sure this is not optimized away (function-scoped -fno-optimize-sibling-calls)
extern "C" NO_OPTIMIZE
int posix_memalign_hook_dummy(void** memptr, size_t alignment, size_t size) {
    return _orig_posix_memalign(memptr, alignment, size);
}

extern "C" void* aligned_alloc_hook(size_t alignment, size_t size) {
    void* ret = _orig_aligned_alloc(alignment, size);
    if (MallocTracer::running() && ret && size) {
        MallocTracer::recordMalloc(ret, size);
    }
    return ret;
}

u64 MallocTracer::_interval;
bool MallocTracer::_nofree;
volatile u64 MallocTracer::_allocated_bytes;

Mutex MallocTracer::_patch_lock;
int MallocTracer::_patched_libs = 0;
bool MallocTracer::_initialized = false;
volatile bool MallocTracer::_running = false;

static pthread_t _current_thread;
static bool _nested_malloc = false;

// Test if calloc() implementation calls malloc()
static void* nested_malloc_hook(size_t size) {
    if (pthread_self() == _current_thread) {
        _nested_malloc = true;
    }
    return _orig_malloc(size);
}

// In some implementations, specifically on musl, calloc() calls malloc() internally,
// and posix_memalign() calls aligned_alloc(). Detect such cases to prevent double-accounting.
static void detectNestedMalloc() {
    CodeCache* libc = Profiler::instance()->findLibraryByAddress((void*)_orig_calloc);
    if (libc == NULL) {
        return;
    }

    libc->patchImport(im_malloc, (void*)nested_malloc_hook);

    _current_thread = pthread_self();
    free(_orig_calloc(1, 1));
    _current_thread = pthread_t(0);
}

// Call each intercepted function at least once to ensure
// its GOT entry is updated with a correct target address
static void resolveMallocSymbols() {
    static volatile intptr_t sink;

    void* p0 = malloc(1);
    void* p1 = realloc(p0, 2);
    void* p2 = calloc(1, 1);
    void* p3 = aligned_alloc(1, 1);
    void* p4 = NULL;
    if (posix_memalign(&p4, sizeof(void*), sizeof(void*)) == 0) free(p4);
    free(p3);
    free(p2);
    free(p1);

    sink = (intptr_t)p1 + (intptr_t)p2 + (intptr_t)p3 + (intptr_t)p4;
}

void MallocTracer::initialize() {
    CodeCache* lib = Profiler::instance()->findLibraryByAddress((void*)MallocTracer::initialize);
    assert(lib);

    resolveMallocSymbols();

    SAVE_IMPORT(malloc);
    SAVE_IMPORT(free);
    SAVE_IMPORT(calloc);
    SAVE_IMPORT(realloc);
    SAVE_IMPORT(posix_memalign);
    SAVE_IMPORT(aligned_alloc);

    detectNestedMalloc();

    lib->mark(
        [](const char* s) -> bool {
            return strcmp(s, "malloc_hook") == 0
                || strcmp(s, "calloc_hook") == 0
                || strcmp(s, "realloc_hook") == 0
                || strcmp(s, "free_hook") == 0
                || strcmp(s, "posix_memalign_hook") == 0
                || strcmp(s, "aligned_alloc_hook") == 0;
        },
        MARK_ASYNC_PROFILER);
}

// To avoid complexity in hooking and tracking reentrancy, a TLS-based approach is not used.
// Reentrant allocation calls would result in double-accounting. However, this does not impact
// the leak detector, as it correctly tracks memory as freed regardless of how many times
// recordMalloc is called with the same address.
void MallocTracer::patchLibraries() {
    MutexLocker ml(_patch_lock);

    CodeCacheArray* native_libs = Profiler::instance()->nativeLibs();
    int native_lib_count = native_libs->count();

    while (_patched_libs < native_lib_count) {
        CodeCache* cc = (*native_libs)[_patched_libs++];

        UnloadProtection handle(cc);
        if (!handle.isValid()) {
            continue;
        }

        cc->patchImport(im_malloc, (void*)malloc_hook);
        cc->patchImport(im_realloc, (void*)realloc_hook);
        cc->patchImport(im_free, (void*)free_hook);
        cc->patchImport(im_aligned_alloc, (void*)aligned_alloc_hook);

        if (_nested_malloc) {
            // Use dummy hooks to prevent double-accounting. Dummy frames from AP are introduced
            // to preserve the frame link to the original caller (see #1226).
            cc->patchImport(im_calloc, (void*)calloc_hook_dummy);
            cc->patchImport(im_posix_memalign, (void*)posix_memalign_hook_dummy);
        } else {
            cc->patchImport(im_calloc, (void*)calloc_hook);
            cc->patchImport(im_posix_memalign, (void*)posix_memalign_hook);
        }
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
    _nofree = args._nofree;
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
