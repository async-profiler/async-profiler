/*
 * Copyright 2023 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "hooks.h"
#include "asprof.h"
#include "os.h"
#include "perfEvents.h"
#include "profiler.h"


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

    int threadId = OS::threadId();
    PerfEvents::createForThread(threadId);
    Log::debug("thread_start: %d", threadId);

    void* result = start_routine(arg);

    PerfEvents::destroyForThread(threadId);
    Log::debug("thread_end: %d", threadId);
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
    int threadId = OS::threadId();
    PerfEvents::destroyForThread(threadId);
    Log::debug("thread_exit: %d", threadId);

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

extern "C" DLLEXPORT
int pthread_create(pthread_t* thread, const pthread_attr_t* attr, ThreadFunc start_routine, void* arg) {
    if (_orig_pthread_create == NULL) {
        _orig_pthread_create = (pthread_create_t)dlsym(RTLD_NEXT, "pthread_create");
        Hooks::init(false);
    }
    return pthread_create_hook(thread, attr, start_routine, arg);
}

extern "C" DLLEXPORT
void pthread_exit(void* retval) {
    if (_orig_pthread_exit == NULL) {
        _orig_pthread_exit = (pthread_exit_t)dlsym(RTLD_NEXT, "pthread_exit");
    }
    pthread_exit_hook(retval);
    abort();  // to suppress gcc warning
}

extern "C" DLLEXPORT
void* dlopen(const char* filename, int flags) {
    if (_orig_dlopen == NULL) {
        _orig_dlopen = (dlopen_t)dlsym(RTLD_NEXT, "dlopen");
        Hooks::init(false);
    }
    return dlopen_hook_impl(filename, flags, false);
}


Mutex Hooks::_patch_lock;
int Hooks::_patched_libs = 0;
bool Hooks::_initialized = false;

void Hooks::init(bool attach) {
    if (!__sync_bool_compare_and_swap(&_initialized, false, true)) {
        return;
    }

    Profiler* profiler = Profiler::instance();
    profiler->updateSymbols(false);
    Profiler::setupSignalHandlers();

    atexit(shutdown);

    if (attach) {
        _orig_pthread_create = pthread_create;
        _orig_pthread_exit = pthread_exit;
        _orig_dlopen = dlopen;
        patchLibraries();
    } else {
        startProfiler();
    }
}

void Hooks::startProfiler() {
    const char* command = getenv("ASPROF_COMMAND");
    if (command == NULL) {
        return;
    }

    Error error = _global_args.parse(command);

    Log::open(_global_args);

    if (error || (error = Profiler::instance()->run(_global_args))) {
        Log::error("%s", error.message());
    }
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
