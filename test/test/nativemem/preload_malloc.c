/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asprof.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

asprof_error_str_t _asprof_error_str;
asprof_execute_t _asprof_execute;
asprof_init_t _asprof_init;

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


void outputCallback(const char* buffer, size_t size) {
    fwrite(buffer, sizeof(char), size, stdout);
}

void executeAsyncProfilerCommand(char* cmd) {
    asprof_error_t asprof_err = _asprof_execute(cmd, outputCallback);
    if (asprof_err != NULL) {
        fprintf(stderr, "%s\n", _asprof_error_str(asprof_err));
        exit(1);
    }
}

void initAsyncProfiler() {
    void* libprof = openLib("libasyncProfiler.so");

    _asprof_init = (asprof_init_t)getSymbol(libprof, "asprof_init");
    _asprof_init();

    _asprof_execute = (asprof_execute_t)getSymbol(libprof, "asprof_execute");
    _asprof_error_str = (asprof_error_str_t)getSymbol(libprof, "asprof_error_str");
}

void preloadOrderTest() {
    unsigned char* ptr = (unsigned char*)malloc(1999993);

    for (int i = 0; i < 1999993; i++) {
        if (ptr[i] != 0xff) {
            fprintf(stderr, "malloc error, expected 0xff but found 0x%x\n", ptr[i]);
            exit(1);
        }
    }
    free(ptr);
}

void apiTest(char* filename) {
    initAsyncProfiler();
    
    char start_cmd[2048] = {0};
    snprintf(start_cmd, sizeof(start_cmd), "start,nativemem,file=%s", filename);
    executeAsyncProfilerCommand(start_cmd);

    preloadOrderTest();

    executeAsyncProfilerCommand("stop");
}

int main(int argc, char** args) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <preload|api> <output>\n", args[0]);
        exit(1);
    }

    int preload = strcmp(args[1], "preload") == 0 ? 1 : 0;

    if (preload) {
        preloadOrderTest();
    } else {
        apiTest(args[2]);
    }

}
