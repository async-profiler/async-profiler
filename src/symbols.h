/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include "codeCache.h"
#include "mutex.h"


class Symbols {
  private:
    static Mutex _parse_lock;
    static bool _have_kernel_symbols;

  public:
    static void parseKernelSymbols(CodeCache* cc);
    static void parseLibraries(CodeCacheArray* array, bool kernel_symbols);

    static bool haveKernelSymbols() {
        return _have_kernel_symbols;
    }
};

#endif // _SYMBOLS_H
