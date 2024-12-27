/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DEMANGLE_H
#define _DEMANGLE_H

struct demangle;

class Demangle {
  private:
    static char* demangleCpp(const char* s);
    static char* demangleRust(struct demangle const *demangle, bool full_signature);
    static bool isRustSymbol(const char* s);
    static void cutArguments(char* s);

  public:
    static char* demangle(const char* s, bool full_signature);

    static bool needsDemangling(const char* s) {
        return s[0] == '_' && (s[1] == 'R' || s[1] == 'Z');
    }
};

#endif // _DEMANGLE_H
