/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include "asprof.h"

#ifdef __linux__
#define EXT ".so"
#else
#define EXT ".dylib"
#endif

static void fail(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

extern void testMethod() {
    printf("Calling testMethod\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fail("Too few arguments");
    }

    void* lib = dlopen("build/lib/libasyncProfiler" EXT, RTLD_NOW);
    if (lib == NULL) {
        fail("Failed to load libasyncProfiler");
    }

    asprof_init_t asprof_init = (asprof_init_t)dlsym(lib, "asprof_init");
    asprof_init();

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "start,nativemem=0,begin=testMethod,end=testMethod,file=%s", argv[1]);

    asprof_execute_t asprof_execute = (asprof_execute_t)dlsym(lib, "asprof_execute");
    asprof_error_t err = asprof_execute(cmd, NULL);
    if (err) {
        fprintf(stderr, "%s\n", err);
    }

    testMethod();
    free(malloc(199999));

    err = asprof_execute("stop", NULL);
    if (err) {
        fprintf(stderr, "%s\n", err);
    }

    return 0;
}
