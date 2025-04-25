/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asprof.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSERT_NO_DLERROR()           \
    err = dlerror();                  \
    if (err != NULL) {                \
        fprintf(stderr, "%s\n", err); \
        exit(1);                      \
    }

#define ASSERT_NO_ASPROF_ERR(err)                       \
    if (err != NULL) {                                  \
        fprintf(stderr, "%s\n", asprof_error_str(err)); \
        exit(1);                                        \
    }

void outputCallback(const char* buffer, size_t size) {
    fwrite(buffer, sizeof(char), size, stdout);
}

/*
Idea of the test (the behavior defined above applies as of 0c72a8d):
- We load libcallsmalloc.so
- We start AP without nativemem mode
    => 'malloc' is not hooked
- We dlclose libcallsmalloc.so
    => The previously accessible memory location dedicated to libcallsmalloc.so becomes inaccessible
- We restart AP in nativemem mode 
    => we try to hook 'malloc'
- AP remembers the previous instance libcallsmalloc.so
    => AP tries to patch an inaccessible memory location
    => SEGFAULT
*/
int main() {
    char *err;

    void* libprof = dlopen("libasyncProfiler.so", RTLD_NOW);
    ASSERT_NO_DLERROR();

    // TODO: Fix for Alpine when discussion is settled
    ((asprof_init_t)dlsym(libprof, "asprof_init"))();
    ASSERT_NO_DLERROR();

    asprof_execute_t asprof_execute = (asprof_execute_t)dlsym(libprof, "asprof_execute");
    ASSERT_NO_DLERROR();

    asprof_error_str_t asprof_error_str = (asprof_error_str_t)dlsym(libprof, "asprof_error_str");
    ASSERT_NO_DLERROR();

    void* lib = dlopen("libcallsmalloc.so", RTLD_NOW | RTLD_GLOBAL);
    ASSERT_NO_DLERROR();

    asprof_error_t asprof_err = asprof_execute("start,cstack=dwarf,collapsed", outputCallback);
    ASSERT_NO_ASPROF_ERR(asprof_err);

    asprof_err = asprof_execute("stop", NULL);
    ASSERT_NO_ASPROF_ERR(asprof_err);

    dlclose(lib);

    asprof_err = asprof_execute("start,nativemem,cstack=dwarf,collapsed", outputCallback);
    ASSERT_NO_ASPROF_ERR(asprof_err);

    asprof_err = asprof_execute("stop", NULL);
    ASSERT_NO_ASPROF_ERR(asprof_err);
}
