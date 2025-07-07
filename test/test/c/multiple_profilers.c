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

asprof_error_str_t _asprof_error_str;
asprof_execute_t _asprof_execute;
asprof_init_t _asprof_init;

void (*pthread_exit_ref)(void*) = pthread_exit;
int (*pthread_create_ref)(pthread_t*, pthread_attr_t*,void*(*)(void*),void *) = pthread_create;

void* openLib(char* name) {
    void* ptr = dlopen(name, RTLD_NOW);
    if (ptr == NULL) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        exit(1);
    }
    return ptr;
}

void* getSymbol(void* lib, char* name) {
    void* ptr = dlsym(lib, name);
    if (ptr == NULL) {
        fprintf(stderr, "dlsym error: %s\n", dlerror());
        exit(1);
    }
    return ptr;
}

void executeAsyncProfilerCommand(char* cmd) {
    asprof_error_t asprof_err = _asprof_execute(cmd, NULL);
    if (asprof_err != NULL) {
        fprintf(stderr, "%s\n", _asprof_error_str(asprof_err));
        exit(1);
    }
}

void initAsyncProfiler() {
    void* libprof = openLib("build/test/lib/libasyncProfiler" LIB_EXT);

    _asprof_init = (asprof_init_t)getSymbol(libprof, "asprof_init");
    _asprof_init();

    _asprof_execute = (asprof_execute_t)getSymbol(libprof, "asprof_execute");
    _asprof_error_str = (asprof_error_str_t)getSymbol(libprof, "asprof_error_str");
}

void sampleMalloc() {
    void* lib = openLib("build/test/lib/libcallsmalloc" LIB_EXT);

    malloc_t call_malloc = (malloc_t)getSymbol(lib, "call_malloc");
    free(call_malloc(1999993));
}

void sampleWall(unsigned long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) ;
}

void* threadEntry1(void* args) {
    sampleMalloc();
    sampleWall(100);
    pthread_exit(NULL);
}

void* threadEntry2(void* args) {
    sampleMalloc();
    sampleWall(100);
    pthread_exit_ref(NULL);
    return NULL;
}

int main(int argc, char** args) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_file>\n", args[0]);
        exit(1);
    }

    initAsyncProfiler();

    char start_cmd[2048] = {0};
    snprintf(start_cmd, sizeof(start_cmd), "start,wall=10ms,cstack=dwarf,file=%s", args[1]);
    executeAsyncProfilerCommand(start_cmd);

    pthread_t thread1, thread2;

    pthread_create(&thread1, NULL, threadEntry1, NULL);
    pthread_create_ref(&thread2, NULL, threadEntry2, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    executeAsyncProfilerCommand("stop");
}
