/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asprof.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_NO_DLERROR(sym)            \
    if (sym == NULL) {                    \
        err = dlerror();                  \
        if (err != NULL) {                \
            fprintf(stderr, "%s\n", err); \
            exit(1);                      \
        }                                 \
    }                                     \

#define ASSERT_NO_ASPROF_ERR(err)                       \
    if (err != NULL) {                                  \
        fprintf(stderr, "%s\n", asprof_error_str(err)); \
        exit(1);                                        \
    }

typedef void* (*call_malloc_t)(size_t);

void outputCallback(const char* buffer, size_t size) {
    fwrite(buffer, sizeof(char), size, stdout);
}

int main(int argc, char** argv) {
    char* err = NULL;
    void* lib = NULL;

    // first arg is the filename and required.
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <dlopen_first | profile_first> <output.jfr>\n", argv[0]);
        exit(1);
    }

    int dlopen_first = strcmp(argv[1], "dlopen_first") == 0 ? 1 : 0;
    const char* filename = argv[2];

    void* libprof = dlopen("libasyncProfiler.so", RTLD_NOW);
    ASSERT_NO_DLERROR(libprof);

    asprof_init_t asprof_init = ((asprof_init_t)dlsym(libprof, "asprof_init"));
    ASSERT_NO_DLERROR(asprof_init);
    asprof_init();

    asprof_execute_t asprof_execute = (asprof_execute_t)dlsym(libprof, "asprof_execute");
    ASSERT_NO_DLERROR(asprof_execute);

    asprof_error_str_t asprof_error_str = (asprof_error_str_t)dlsym(libprof, "asprof_error_str");
    ASSERT_NO_DLERROR(asprof_error_str);

    // Load libcallsmalloc.so before or after starting the profiler, based on args.
    if (dlopen_first) {
        lib = dlopen("libcallsmalloc.so", RTLD_NOW);
        ASSERT_NO_DLERROR(lib);
    }

    // Start profiler.
    char start_cmd[2048] = {0};
    snprintf(start_cmd, sizeof(start_cmd), "start,nativemem,cstack=dwarf,file=%s", filename);

    asprof_error_t asprof_err = asprof_execute(start_cmd, outputCallback);
    ASSERT_NO_ASPROF_ERR(asprof_err);

    if (!dlopen_first) {
        lib = dlopen("libcallsmalloc.so", RTLD_NOW);
        ASSERT_NO_DLERROR(lib);
    }

    call_malloc_t call_malloc = (call_malloc_t)dlsym(lib, "call_malloc");
    ASSERT_NO_DLERROR(call_malloc);

    free(call_malloc(1999993));

    asprof_err = asprof_execute("stop", NULL);
    ASSERT_NO_ASPROF_ERR(err);
}
