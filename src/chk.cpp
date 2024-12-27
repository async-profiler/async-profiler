/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __clang__

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "asprof.h"


// libgcc refers to __sprintf_chk, but there is no such symbol in musl libc.
// Export a weak symbol in order to make profiler library work both with glibc and musl.

extern "C" WEAK DLLEXPORT
int __sprintf_chk(char* s, int flag, size_t slen, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(s, slen, format, args);
    va_end(args);

    if (ret >= slen) {
        fprintf(stderr, "__sprintf_chk failed\n");
        abort();
    }

    return ret;
}

#endif // __clang__
