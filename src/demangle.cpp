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

bool Demangle::isRustSymbol(const char* s) {
    // "_R" symbols (Rust "mangling V0") symbols can always be easily distinguished from C++ symbols.
    if (s[0] == '_' && s[1] == 'R') {
        return true;
    }

    // Rust symbols with the legacy demangling (`_ZN3foo3bar17h0123456789abcdefE`) look very much like valid
    // C++ demangling symbols, but we only want to use the Rust demangling for Rust symbols since
    // the Rust demangling does not support C++ anonymous namespaces (e.g. `_ZN12_GLOBAL__N_113single_threadE`
    // is supposed to demangle to `(anonymous namespace)::single_thread`, but Rust will demangle it to
    // `_GLOBAL__N_1::single_thread`).
    //
    // So try to have a heuristic to avoid sending C++ symbols to Rust demangling - if a symbol's last "E"
    // refers to a Rust hash, expect it to be a Rust symbol. We don't require the `E` to be at the end
    // of the string since there can be `.lto.1` suffixes.
    //
    // FIXME: there might be a better heuristic if there are problems with this one
    const char* e = strrchr(s, 'E');
    if (e != NULL && e - s > 22 && e[-19] == '1' && e[-18] == '7' && e[-17] == 'h') {
        const char* h = e - 16;
        while ((*h >= '0' && *h <= '9') || (*h >= 'a' && *h <= 'f')) h++;
        if (h == e) {
            return true;
        }
    }

    return false;
}

char* Demangle::demangleRust(struct demangle const *demangle, bool full_signature) {
    for (size_t demangled_size = 64; demangled_size < 1000000; demangled_size *= 2) {
        char* result = (char*)malloc(demangled_size);
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
    if (isRustSymbol(s)) {
        struct demangle demangle;
        rust_demangle_demangle(s, &demangle);
        if (rust_demangle_is_known(&demangle)) {
            return demangleRust(&demangle, full_signature);
        }
    }

    char* result = demangleCpp(s);
    if (result != NULL && !full_signature) {
        cutArguments(result);
    }
    return result;
}
