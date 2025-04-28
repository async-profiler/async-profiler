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
    static bool _libs_limit_reported;
    static const void* _main_phdr;
    static const char* _ld_base;

  public:
    static void parseKernelSymbols(CodeCache* cc);
    static void parseLibraries(CodeCacheArray* array, bool kernel_symbols);

    static bool isMainExecutable(const char* image_base, const void* map_end) {
      return _main_phdr != NULL && _main_phdr >= image_base && _main_phdr < map_end;
    }

    static bool isLoader(const char* image_base) {
      return _ld_base != NULL && _ld_base == image_base;
    }

    static bool haveKernelSymbols() {
        return _have_kernel_symbols;
    }

    Symbols();
};

static Symbols symbols;

#endif // _SYMBOLS_H
