/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asprof.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#ifdef __APPLE__
#define LIB_EXT ".dylib"
#else
#define LIB_EXT ".so"
#endif

typedef void* (*malloc_t)(size_t);

asprof_error_str_t _asprof_error_str[2];
asprof_execute_t _asprof_execute[2];
asprof_init_t _asprof_init[2];

typedef void (*pthread_exit_t)(void*);
typedef void* (*pthread_entry_t)(void*);
typedef int (*pthread_create_t)(pthread_t*, pthread_attr_t*, pthread_entry_t, void*);

// This is to make sure invocations on references in .rela.dyn don't cause infinite loops
pthread_exit_t pthread_exit_ref = (pthread_exit_t)pthread_exit;
pthread_create_t pthread_create_ref = (pthread_create_t)pthread_create;

void* openLib(const char* name) {
    void* ptr = dlopen(name, RTLD_NOW);
    if (ptr == NULL) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        exit(1);
    }
    return ptr;
}

void* getSymbol(void* lib, const char* name) {
    void* ptr = dlsym(lib, name);
    if (ptr == NULL) {
        fprintf(stderr, "dlsym error: %s\n", dlerror());
        exit(1);
    }
    return ptr;
}

void executeAsyncProfilerCommand(int profiler_id, const char* cmd) {
    asprof_error_t asprof_err = _asprof_execute[profiler_id](cmd, NULL);
    if (asprof_err != NULL) {
        fprintf(stderr, "%s\n", _asprof_error_str[profiler_id](asprof_err));
        exit(1);
    }
}

void initAsyncProfiler(int profiler_id) {
    void* libprof;

    if (profiler_id == 0) {
        libprof = openLib("build/test/lib/libasyncProfiler-copy" LIB_EXT);
    } else {
        libprof = openLib("build/lib/libasyncProfiler" LIB_EXT);
    }

    _asprof_init[profiler_id]= (asprof_init_t)getSymbol(libprof, "asprof_init");
    _asprof_init[profiler_id]();

    _asprof_execute[profiler_id] = (asprof_execute_t)getSymbol(libprof, "asprof_execute");
    _asprof_error_str[profiler_id] = (asprof_error_str_t)getSymbol(libprof, "asprof_error_str");
}

void doMalloc() {
    void* lib = openLib("build/test/lib/libcallsmalloc" LIB_EXT);

    malloc_t call_malloc = (malloc_t)getSymbol(lib, "call_malloc");
    free(call_malloc(1999993));
}

void doSleep(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
    // Do sleep for the full duration (can be interrupted by the profiler)
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) ;
}

void* threadEntry1(void* args) {
    doMalloc();
    doSleep(100);
    pthread_exit(NULL);
}

void* threadEntry2(void* args) {
    doMalloc();
    doSleep(1000);
    pthread_exit_ref(NULL);
    return NULL;
}

int main(int argc, char** args) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s api|preload <output_file_1> [output_file_2]\n", args[0]);
        exit(1);
    }

    int api_mode = strcmp(args[1], "api") == 0;
    if (api_mode && argc < 4) {
        fprintf(stderr, "Usage: %s api <output_file_1> <output_file_2>\n", args[0]);
        exit(1);
    }

    initAsyncProfiler(0);

    if (api_mode) {
        initAsyncProfiler(1);
    }

    char start_cmd[2048] = {0};
    snprintf(start_cmd, sizeof(start_cmd), "start,wall=10ms,file=%s", api_mode ? args[3] : args[2]);
    executeAsyncProfilerCommand(0, start_cmd);

    if (api_mode) {
        char start_cmd2[2048] = {0};
        snprintf(start_cmd2, sizeof(start_cmd2), "start,nativemem,file=%s", args[2]);
        executeAsyncProfilerCommand(1, start_cmd2);
    }

    pthread_t thread1, thread2;

    pthread_create(&thread1, NULL, threadEntry1, NULL);
    pthread_create_ref(&thread2, NULL, threadEntry2, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    executeAsyncProfilerCommand(0, "stop");

    if (api_mode) {
        executeAsyncProfilerCommand(1, "stop");
    }
}
