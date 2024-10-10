/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "hooks.h"
#include "asprof.h"
#include "cpuEngine.h"
#include "profiler.h"


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


typedef void* (*dlopen_t)(const char*, int);
static dlopen_t _orig_dlopen = NULL;

static void* dlopen_hook_impl(const char* filename, int flags, bool patch) {
    Log::debug("dlopen: %s", filename);
    void* result = _orig_dlopen(filename, flags);
    if (result != NULL && filename != NULL) {
        Profiler::instance()->updateSymbols(false);
        if (patch) {
            Hooks::patchLibraries();
        }
    }
    return result;
}

static void* dlopen_hook(const char* filename, int flags) {
    return dlopen_hook_impl(filename, flags, true);
}


// LD_PRELOAD hooks

extern "C" WEAK DLLEXPORT
int pthread_create(pthread_t* thread, const pthread_attr_t* attr, ThreadFunc start_routine, void* arg) {
    if (_orig_pthread_create == NULL) {
        _orig_pthread_create = ADDRESS_OF(pthread_create);
    }
    if (Hooks::initialized()) {
        return pthread_create_hook(thread, attr, start_routine, arg);
    }
    return _orig_pthread_create(thread, attr, start_routine, arg);
}

extern "C" WEAK DLLEXPORT
void pthread_exit(void* retval) {
    if (_orig_pthread_exit == NULL) {
        _orig_pthread_exit = ADDRESS_OF(pthread_exit);
    }
    if (Hooks::initialized()) {
        pthread_exit_hook(retval);
    } else {
        _orig_pthread_exit(retval);
    }
    abort();  // to suppress gcc warning
}

extern "C" WEAK DLLEXPORT
void* dlopen(const char* filename, int flags) {
    if (_orig_dlopen == NULL) {
        _orig_dlopen = ADDRESS_OF(dlopen);
    }
    if (Hooks::initialized()) {
        return dlopen_hook_impl(filename, flags, false);
    }
    return _orig_dlopen(filename, flags);
}


Mutex Hooks::_patch_lock;
int Hooks::_patched_libs = 0;
bool Hooks::_initialized = false;

bool Hooks::init(bool attach) {
    if (!__sync_bool_compare_and_swap(&_initialized, false, true)) {
        return false;
    }

    Profiler::instance()->updateSymbols(false);
    Profiler::setupSignalHandlers();

    if (attach) {
        _orig_pthread_create = ADDRESS_OF(pthread_create);
        _orig_pthread_exit = ADDRESS_OF(pthread_exit);
        _orig_dlopen = ADDRESS_OF(dlopen);
        patchLibraries();
    }

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
        cc->patchImport(im_dlopen, (void*)dlopen_hook);
        cc->patchImport(im_pthread_create, (void*)pthread_create_hook);
        cc->patchImport(im_pthread_exit, (void*)pthread_exit_hook);
    }
}
