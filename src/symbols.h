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

  public:
    static void parseKernelSymbols(CodeCache* cc);
    static void parseLibraries(CodeCacheArray* array, bool kernel_symbols);

    static bool haveKernelSymbols() {
        return _have_kernel_symbols;
    }

    Symbols();
};

static Symbols symbols;

class UnloadProtection {
  private:
    CodeCache* _protected_cc;
    void* _lib_handle;
    bool _valid;

  public:
    UnloadProtection(CodeCache *cc);
    ~UnloadProtection();

    UnloadProtection& operator=(const UnloadProtection& other) = delete;

    void patchImport(ImportId id, void* hook_func) const;
    bool isValid() const { return _valid; }
};

#endif // _SYMBOLS_H
