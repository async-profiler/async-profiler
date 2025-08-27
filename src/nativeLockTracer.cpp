/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "codeCache.h"
#include "nativeLockTracer.h"
#include "symbols.h"


extern "C" int pthread_mutex_lock_hook(pthread_mutex_t *mutex) {
    if (!NativeLockTracer::running()) {
        return pthread_mutex_lock(mutex);
    }

    if (pthread_mutex_trylock(mutex) == 0) {
        return 0;
    }

    u64 start_time = OS::nanotime();
    int ret = pthread_mutex_lock(mutex);
    u64 end_time = OS::nanotime();

    NativeLockTracer::recordNativeLock(mutex, start_time, end_time);

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
            return strcmp(s, "pthread_mutex_lock_hook") == 0;
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
    }
}

void NativeLockTracer::recordNativeLock(void* address, u64 start_time, u64 end_time) {
    const u64 duration = end_time - start_time;
    if (updateCounter(_total_duration, duration, _interval)) {
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
