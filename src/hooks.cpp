/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include "hooks.h"
#include "asprof.h"
#include "cpuEngine.h"
#include "mallocTracer.h"
#include "profiler.h"
#include "symbols.h"
#include "vmStructs.h"

#define ADDRESS_OF(sym) ({ \
    void* addr = dlsym(RTLD_NEXT, #sym); \
    addr != NULL ? (sym##_t)addr : sym;  \
})

typedef void* (*ThreadFunc)(void*);

struct ThreadEntry {
    ThreadFunc start_routine;
    void* arg;
};

typedef int (*pthread_create_t)(pthread_t*, const pthread_attr_t*, ThreadFunc, void*);
static pthread_create_t _orig_pthread_create = NULL;

typedef void (*pthread_exit_t)(void*);
static pthread_exit_t _orig_pthread_exit = NULL;

static void unblock_signals() {
    sigset_t set;
    sigemptyset(&set);
    if (_global_args._signal == 0) {
        sigaddset(&set, SIGPROF);
        sigaddset(&set, SIGVTALRM);
    } else {
        for (int s = _global_args._signal; s > 0; s >>= 8) {
            sigaddset(&set, s & 0xff);
        }
    }
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

static void* thread_start_wrapper(void* e) {
    ThreadEntry* entry = (ThreadEntry*)e;
    ThreadFunc start_routine = entry->start_routine;
    void* arg = entry->arg;
    free(entry);

    unblock_signals();

    unsigned long current_thread = (unsigned long)(uintptr_t)pthread_self();
    Log::debug("thread_start: 0x%lx", current_thread);
    CpuEngine::onThreadStart();

    void* result = start_routine(arg);

    Log::debug("thread_end: 0x%lx", current_thread);
    CpuEngine::onThreadEnd();

    return result;
}

static int pthread_create_hook(pthread_t* thread, const pthread_attr_t* attr, ThreadFunc start_routine, void* arg) {
    ThreadEntry* entry = (ThreadEntry*) malloc(sizeof(ThreadEntry));
    entry->start_routine = start_routine;
    entry->arg = arg;

    int result = _orig_pthread_create(thread, attr, thread_start_wrapper, entry);
    if (result != 0) {
        free(entry);
    }
   return result;
}

static void pthread_exit_hook(void* retval) {
    Log::debug("thread_exit: 0x%lx", (unsigned long)(uintptr_t)pthread_self());
    CpuEngine::onThreadEnd();

    _orig_pthread_exit(retval);
}


static void* dlopen_hook_impl(const char* filename, int flags) {
    Log::debug("dlopen: %s", filename);
    void* result = dlopen(filename, flags);
    if (result != NULL && filename != NULL) {
        Profiler::instance()->updateSymbols(false);
        Profiler::installHooks();
    }
    return result;
}

// Intercept thread creation/termination by patching libjvm's GOT entry for pthread_setspecific().
// HotSpot puts VMThread into TLS on thread start, and resets on thread end.
static int pthread_setspecific_hook(pthread_key_t key, const void* value) {
    if (key != VMThread::key()) {
        return pthread_setspecific(key, value);
    }

    if (pthread_getspecific(key) == value) {
        return 0;
    }

    if (value != NULL) {
        int result = pthread_setspecific(key, value);
        CpuEngine::onThreadStart();
        return result;
    } else {
        CpuEngine::onThreadEnd();
        return pthread_setspecific(key, value);
    }
}

Mutex Hooks::_patch_lock;
int Hooks::_patched_libs = 0;
bool Hooks::_initialized = false;

bool Hooks::init() {
    if (!__sync_bool_compare_and_swap(&_initialized, false, true)) {
        return false;
    }

    Profiler::setupSignalHandlers();

    Profiler::instance()->updateSymbols(false);
    _orig_pthread_create = ADDRESS_OF(pthread_create);
    _orig_pthread_exit = ADDRESS_OF(pthread_exit);
    patchLibraries();

    atexit(shutdown);

    return true;
}

void Hooks::shutdown() {
    Profiler::instance()->shutdown(_global_args);
}

void Hooks::patchLibraries() {
    MutexLocker ml(_patch_lock);

    CodeCacheArray* native_libs = Profiler::instance()->nativeLibs();
    int native_lib_count = native_libs->count();

    while (_patched_libs < native_lib_count) {
        CodeCache* cc = (*native_libs)[_patched_libs++];
        UnloadProtection handle(cc);
        if (!handle.isValid()) {
            continue;
        }

        if (!cc->contains((const void*)Hooks::init)) {
            // Let libasyncProfiler always use original dlopen
            cc->patchImport(im_dlopen, (void*)dlopen_hook_impl);
            cc->patchImport(im_pthread_setspecific, (void*)pthread_setspecific_hook);
        }
        cc->patchImport(im_pthread_create, (void*)pthread_create_hook);
        cc->patchImport(im_pthread_exit, (void*)pthread_exit_hook);
    }
}

void Hooks::unpatchLibraries() {
    MutexLocker ml(_patch_lock);

    CodeCacheArray* native_libs = Profiler::instance()->nativeLibs();
    while (_patched_libs > 0) {
        CodeCache* cc = (*native_libs)[--_patched_libs];
        UnloadProtection handle(cc);
        if (!handle.isValid()) {
            continue;
        }
        cc->unpatchImport(im_dlopen);
        cc->unpatchImport(im_pthread_setspecific);
        cc->unpatchImport(im_pthread_create);
        cc->unpatchImport(im_pthread_exit);
    }
}