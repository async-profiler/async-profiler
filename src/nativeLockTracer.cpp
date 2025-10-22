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


extern "C" int pthread_mutex_lock_hook(pthread_mutex_t* mutex) {
    if (!NativeLockTracer::running()) {
        return pthread_mutex_lock(mutex);
    }

    if (pthread_mutex_trylock(mutex) == 0) {
        return 0;
    }

    u64 start_time = TSC::ticks();
    int ret = pthread_mutex_lock(mutex);
    u64 end_time = TSC::ticks();

    if  (ret == 0) {
        NativeLockTracer::recordNativeLock(mutex, start_time, end_time);
    }
    
    return ret;
}

extern "C" int pthread_rwlock_rdlock_hook(pthread_rwlock_t* rwlock) {
    if (!NativeLockTracer::running()) {
        return pthread_rwlock_rdlock(rwlock);
    }

    if (pthread_rwlock_tryrdlock(rwlock) == 0) {
        return 0;
    }

    u64 start_time = TSC::ticks();
    int ret = pthread_rwlock_rdlock(rwlock);
    u64 end_time = TSC::ticks();

    if  (ret == 0) {
        NativeLockTracer::recordNativeLock(rwlock, start_time, end_time);
    }

    return ret;
}

extern "C" int pthread_rwlock_wrlock_hook(pthread_rwlock_t* rwlock) {
    if (!NativeLockTracer::running()) {
        return pthread_rwlock_wrlock(rwlock);
    }

    if (pthread_rwlock_trywrlock(rwlock) == 0) {
        return 0;
    }

    u64 start_time = TSC::ticks();
    int ret = pthread_rwlock_wrlock(rwlock);
    u64 end_time = TSC::ticks();

    if  (ret == 0) {
        NativeLockTracer::recordNativeLock(rwlock, start_time, end_time);
    }

    return ret;
}


u64 NativeLockTracer::_interval;
double NativeLockTracer::_ticks_to_nanos;
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

    while (_patched_libs < native_lib_count) {
        CodeCache* cc = (*native_libs)[_patched_libs++];

        if (cc->contains((const void*)NativeLockTracer::initialize)) {
            continue;
        }

        UnloadProtection handle(cc);
        if (!handle.isValid()) {
            continue;
        }

        cc->patchImport(im_pthread_mutex_lock, (void*)pthread_mutex_lock_hook);
        cc->patchImport(im_pthread_rwlock_rdlock, (void*)pthread_rwlock_rdlock_hook);
        cc->patchImport(im_pthread_rwlock_wrlock, (void*)pthread_rwlock_wrlock_hook);
    }
}

void NativeLockTracer::recordNativeLock(void* address, u64 start_time, u64 end_time) {
    const u64 duration_ticks = end_time - start_time;
    if (updateCounter(_total_duration, duration_ticks, _interval)) {
        u64 duration_nanos = (u64)(duration_ticks * _ticks_to_nanos);
        NativeLockEvent event;
        event._start_time = start_time;
        event._end_time = end_time;
        event._address = (uintptr_t)address;

        Profiler::instance()->recordSample(NULL, duration_nanos, NATIVE_LOCK_SAMPLE, &event);
    }
}

Error NativeLockTracer::start(Arguments& args) {
    _ticks_to_nanos = 1e9 / TSC::frequency();
    _interval = (u64)(args._nativelock * (TSC::frequency() / 1e9));
    _total_duration = 0;

    if (!_initialized) {
        initialize();
        _initialized = true;
    }

    _running = true;
    patchLibraries();

    return Error::OK;
}

void NativeLockTracer::stop() {
    _running = false;
}
