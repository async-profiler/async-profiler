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

char* Demangle::demangleRust(const char* s, const char* e) {
    // Demangled symbol can be 1.5x longer than original, e.g. 1A1B1C -> A::B::C
    char* result = (char*)malloc((e - s) * 3 / 2 + 1);
    if (result == NULL) {
        return NULL;
    }

    char* r = result;
    char* tmp;

    while (s < e) {
        unsigned long len = strtoul(s, &tmp, 10);
        const char* next = tmp + len;
        if (len == 0 || next > e) {
            break;
        }

        s = tmp;
        if (s[0] == '_' && s[1] == '$') s++;

        if (r > result) {
            *r++ = ':';
            *r++ = ':';
        }

        while (s < next) {
            if (s[0] == '$') {
                if (s[1] == 'L' && s[2] == 'T' && s[3] == '$') {
                    *r++ = '<';
                    s += 4;
                } else if (s[1] == 'G' && s[2] == 'T' && s[3] == '$') {
                    *r++ = '>';
                    s += 4;
                } else if (s[1] == 'L' && s[2] == 'P' && s[3] == '$') {
                    *r++ = '(';
                    s += 4;
                } else if (s[1] == 'R' && s[2] == 'P' && s[3] == '$') {
                    *r++ = ')';
                    s += 4;
                } else if (s[1] == 'S' && s[2] == 'P' && s[3] == '$') {
                    *r++ = '@';
                    s += 4;
                } else if (s[1] == 'B' && s[2] == 'P' && s[3] == '$') {
                    *r++ = '*';
                    s += 4;
                } else if (s[1] == 'R' && s[2] == 'F' && s[3] == '$') {
                    *r++ = '&';
                    s += 4;
                } else if (s[1] == 'C' && s[2] == '$') {
                    *r++ = ',';
                    s += 3;
                } else if (s[1] == 'u') {
                    *r++ = (char)strtoul(s + 2, &tmp, 16);
                    s = tmp + 1;
                } else {
                    *r++ = '$';
                    s++;
                }
            } else if (s[0] == '.' && s[1] == '.') {
                *r++ = ':';
                *r++ = ':';
                s += 2;
            } else {
                *r++ = *s++;
            }
        }

        if (s > next) {
            break;
        }
    }

    *r = 0;
    return result;
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
    // Check if the mangled symbol ends with a Rust hash "17h<hex>E"
    const char* e = strrchr(s, 'E');
    if (e != NULL && e - s > 22 && e[-19] == '1' && e[-18] == '7' && e[-17] == 'h') {
        const char* h = e - 16;
        while ((*h >= '0' && *h <= '9') || (*h >= 'a' && *h <= 'f')) h++;
        if (h == e) {
            return demangleRust(s + 3, e - 19);
        }
    }

    char* result = demangleCpp(s);
    if (result != NULL && !full_signature) {
        cutArguments(result);
    }
    return result;
}
