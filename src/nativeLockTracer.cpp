/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include "codeCache.h"
#include "nativeLockTracer.h"
#include "profiler.h"
#include "symbols.h"
#include "tsc.h"


extern "C" int pthread_mutex_lock_hook(pthread_mutex_t *mutex) {
    static int hook_call_count = 0;
    if (++hook_call_count <= 5) {  // Log first 5 calls only
        Log::info("NativeLockTracer: pthread_mutex_lock_hook called #%d", hook_call_count);
    }
    
    if (!NativeLockTracer::running()) {
        return pthread_mutex_lock(mutex);
    }

    if (pthread_mutex_trylock(mutex) == 0) {
        return 0;
    }

    u64 start_time = TSC::ticks();
    int ret = pthread_mutex_lock(mutex);
    u64 end_time = TSC::ticks();

    NativeLockTracer::recordNativeLock(mutex, start_time, end_time);

    return ret;
}

extern "C" int pthread_rwlock_rdlock_hook(pthread_rwlock_t *rwlock) {
    if (!NativeLockTracer::running()) {
        return pthread_rwlock_rdlock(rwlock);
    }

    if (pthread_rwlock_tryrdlock(rwlock) == 0) {
        return 0;
    }

    u64 start_time = TSC::ticks();
    int ret = pthread_rwlock_rdlock(rwlock);
    u64 end_time = TSC::ticks();

    NativeLockTracer::recordNativeLock(rwlock, start_time, end_time);

    return ret;
}

extern "C" int pthread_rwlock_wrlock_hook(pthread_rwlock_t *rwlock) {
    if (!NativeLockTracer::running()) {
        return pthread_rwlock_wrlock(rwlock);
    }

    if (pthread_rwlock_trywrlock(rwlock) == 0) {
        return 0;
    }

    u64 start_time = TSC::ticks();
    int ret = pthread_rwlock_wrlock(rwlock);
    u64 end_time = TSC::ticks();

    NativeLockTracer::recordNativeLock(rwlock, start_time, end_time);

    return ret;
}


u64 NativeLockTracer::_interval;
Mutex NativeLockTracer::_patch_lock;
int NativeLockTracer::_patched_libs = 0;
bool NativeLockTracer::_initialized = false;
volatile bool NativeLockTracer::_running = false;
volatile u64 NativeLockTracer::_total_duration;  // for interval sampling

void NativeLockTracer::initialize() {
    CodeCache* lib = Profiler::instance()->findLibraryByAddress((void*)NativeLockTracer::initialize);
    assert(lib);

    lib->mark(
        [](const char* s) -> bool {
            return strcmp(s, "pthread_mutex_lock_hook") == 0
                || strcmp(s, "pthread_rwlock_rdlock_hook") == 0
                || strcmp(s, "pthread_rwlock_wrlock_hook") == 0;
        },
        MARK_ASYNC_PROFILER);
}

void NativeLockTracer::patchLibraries() {
    MutexLocker ml(_patch_lock);

    CodeCacheArray* native_libs = Profiler::instance()->nativeLibs();
    int native_lib_count = native_libs->count();

    Log::info("NativeLockTracer: Patching %d libraries", native_lib_count);

    while (_patched_libs < native_lib_count) {
        CodeCache* cc = (*native_libs)[_patched_libs++];

        if (cc->contains((const void*)NativeLockTracer::initialize)) {
            Log::info("NativeLockTracer: Skipping self-library %s", cc->name());
            continue;
        }

        UnloadProtection handle(cc);
        if (!handle.isValid()) {
            Log::info("NativeLockTracer: Invalid handle for library %s", cc->name());
            continue;
        }

        Log::info("NativeLockTracer: Patching library %s", cc->name());
        
        // Check if imports exist before patching
        void** mutex_import = cc->findImport(im_pthread_mutex_lock);
        void** rdlock_import = cc->findImport(im_pthread_rwlock_rdlock);
        void** wrlock_import = cc->findImport(im_pthread_rwlock_wrlock);
        
        Log::info("NativeLockTracer: Library %s imports - mutex:%s rdlock:%s wrlock:%s", 
                  cc->name(),
                  mutex_import ? "FOUND" : "MISSING",
                  rdlock_import ? "FOUND" : "MISSING",
                  wrlock_import ? "FOUND" : "MISSING");
        
        cc->patchImport(im_pthread_mutex_lock, (void*)pthread_mutex_lock_hook);
        cc->patchImport(im_pthread_rwlock_rdlock, (void*)pthread_rwlock_rdlock_hook);
        cc->patchImport(im_pthread_rwlock_wrlock, (void*)pthread_rwlock_wrlock_hook);
        
        Log::info("NativeLockTracer: Library %s patching completed", cc->name());
    }
}

void NativeLockTracer::recordNativeLock(void* address, u64 start_time, u64 end_time) {
    const u64 duration = end_time - start_time;
    static int sample_count = 0;
    
    if (updateCounter(_total_duration, duration, _interval)) {
        sample_count++;
        if (sample_count <= 5) {  // Log first 5 samples
            Log::info("NativeLockTracer: Recording sample #%d, duration=%llu", sample_count, duration);
        }
        
        NativeLockEvent event;
        event._start_time = start_time;
        event._address = (uintptr_t)address;
        event._end_time = end_time;

        Profiler::instance()->recordSample(NULL, duration, NATIVE_LOCK_SAMPLE, &event);
    }
}

Error NativeLockTracer::start(Arguments& args) {
    _interval = args._nativelock > 0 ? args._nativelock : 0;
    _total_duration = 0;

    Log::info("NativeLockTracer: Starting with interval=%llu", _interval);

    if (!_initialized) {
        Log::info("NativeLockTracer: Initializing");
        initialize();
        _initialized = true;
    }

    _running = true;
    Log::info("NativeLockTracer: Running, about to patch libraries");
    patchLibraries();
    Log::info("NativeLockTracer: Library patching complete");

    return Error::OK;
}

void NativeLockTracer::stop() {
    _running = false;
}
