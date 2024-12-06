/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cxxabi.h>
#include <stdlib.h>
#include <string.h>
#include "demangle.h"
#include "rustDemangle.h"

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

char* Demangle::demangleRust(struct demangle const *demangle, bool full_signature) {
    char* result;
    for (size_t demangled_size = 64; demangled_size < 1000000; demangled_size *= 2) {
        result = (char *)malloc(demangled_size);
        if (result == NULL) {
            return NULL;
        }
        if (rust_demangle_display_demangle(demangle, result, demangled_size, !full_signature /* alternate */) == OverflowOk) {
            return result;
        }
        free(result);
    }
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
    // Heuristic: check suffix_len == 0 or that there is a dot in the suffix to make sure the Rust demangled part
    // contains the entire symbol, otherwise a C++ symbol can look like a Rust symbol with a suffix,
    // e.g. `_ZN5MyMapESt6vectorIRKSsE`.
    //
    // Itanium names can't contain an unescaped "." so this should be fine.
    //
    // Theoretically, non-Rust Itanium could also generate a symbol that is like `_ZN5MyMapE` which would be detected
    // as a Rust symbol, but in that case the demangling would be identical.
    if (rust_demangle_is_known(&demangle) && (demangle.suffix_len == 0 || demangle.suffix[0] == '.')) {
        return demangleRust(&demangle, full_signature);
    }

    char* result = demangleCpp(s);
    if (result != NULL && !full_signature) {
        cutArguments(result);
    }
    return result;
}
