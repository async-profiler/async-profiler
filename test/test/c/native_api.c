/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "asprof.h"

#ifdef __linux__
const char profiler_lib_path[] = "build/lib/libasyncProfiler.so";
const char profiler_start_cmd[] = "start,event=cpu,interval=1ms,wall=10ms,cstack=dwarf,loglevel=debug,file=%s";
#else
const char profiler_lib_path[] = "build/lib/libasyncProfiler.dylib";
const char profiler_start_cmd[] = "start,event=cpu,interval=1ms,cstack=dwarf,loglevel=debug,file=%s";
#endif

static void fail(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void uninterruptible_sleep(unsigned long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) ;
}


int main(int argc, char** argv) {
    if (argc < 2) {
        fail("Too few arguments");
    }

    void* lib = dlopen(profiler_lib_path, RTLD_NOW);
    if (lib == NULL) {
        fail("Failed to load libasyncProfiler");
    }

    asprof_init_t asprof_init = dlsym(lib, "asprof_init");
    asprof_init();

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), profiler_start_cmd, argv[1]);

    printf("Starting profiler\n");
    asprof_execute_t asprof_execute = dlsym(lib, "asprof_execute");
    asprof_error_t err = asprof_execute(cmd, NULL);
    if (err != NULL) {
        fail(err);
    }

    uninterruptible_sleep(2000);

    printf("Stopping profiler\n");
    err = asprof_execute("stop", NULL);
    if (err != NULL) {
        fail(err);
    }

    return 0;
}
