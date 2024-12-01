/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cxxabi.h>
#include <stdlib.h>
#include <string.h>
#include "demangle.h"


char* Demangle::demangleCpp(const char* s) {
    int status;
    char* result = abi::__cxa_demangle(s, NULL, NULL, &status);
    if (result == NULL && status == -2) {
        // Strip compiler-specific suffix (e.g. ".part.123") and retry demangling
        char buf[512];
        const char* p = strchr(s, '.');
        if (p != NULL && p - s < sizeof(buf)) {
            memcpy(buf, s, p - s);
            buf[p - s] = 0;
            result = abi::__cxa_demangle(buf, NULL, NULL, &status);
        }
    }
    return result;
}

char* Demangle::demangleRust(const char *s, struct demangle const *demangle) {
    // Demangled symbol can be 1.5x longer than original, e.g. 1A1B1C -> A::B::C
    char* result = NULL, *new_result;
    for (size_t demangled_size = strlen(s) * 3 / 2 + 1; demangled_size < 1000000; demangled_size *= 2) {
        new_result = (char *)realloc(result, demangled_size);
        if (new_result == NULL) {
            free(result);
            return NULL;
        }
        result = new_result;
        if (rust_demangle_display_demangle(demangle, result, demangled_size, true /* alternate */) == OverflowOk) {
            return result;
        }
    }
    free(result);
    // demangling Rust failed, return NULL
    return NULL;
}

void Demangle::cutArguments(char* s) {
    char* p = strrchr(s, ')');
    if (p == NULL) return;

    int balance = 1;
    while (--p > s) {
        if (*p == '(' && --balance == 0) {
            *p = 0;
            return;
        } else if (*p == ')') {
            balance++;
        }
    }
}

char* Demangle::demangle(const char* s, bool full_signature) {
    // try to demangle as Rust
    struct demangle demangle;
    rust_demangle_demangle(s, &demangle);
    if (rust_demangle_is_known(&demangle)) {
        return demangleRust(s, &demangle);
    }

    char* result = demangleCpp(s);
    if (result != NULL && !full_signature) {
        cutArguments(result);
    }
    return result;
}
