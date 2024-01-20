/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DEMANGLE_H
#define _DEMANGLE_H


class Demangle {
  private:
    static char* demangleCpp(const char* s);
    static char* demangleRust(const char* s, const char* e);
    static void cutArguments(char* s);

  public:
    static char* demangle(const char* s, bool full_signature);
};

#endif // _DEMANGLE_H
