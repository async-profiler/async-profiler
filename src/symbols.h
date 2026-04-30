/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include <vector>
#include "codeCache.h"
#include "mutex.h"


class Symbols {
  private:
    static Mutex _parse_lock;
    static bool _have_kernel_symbols;
    static bool _libs_limit_reported;
    static std::vector<const char*> _includes;
    static std::vector<const char*> _excludes;

  public:
    static void parseKernelSymbols(CodeCache* cc);
    static void parseLibraries(CodeCacheArray* array, bool kernel_symbols);

    // When set true, parseLibraries only parses libjvm and skips everything
    // else. Used during static initialization (LateInitializer) to find
    // AsyncGetCallTrace without paying the full library-walk cost on processes
    // with thousands of native libraries.
    static void setEagerParseLibjvmOnly(bool on);

    static bool haveKernelSymbols() {
        return _have_kernel_symbols;
    }

    // Configure library-level filter. Patterns are fnmatch(3) globs matched against
    // the library basename. Pointers must remain valid for the profile session
    // (Arguments owns the backing storage).
    //
    // Semantics: this is purely a *parse-time* filter. Libraries are never removed
    // from the CodeCache once parsed — the cache is additive across profile
    // sessions. Changing the filter on a subsequent attach affects only newly
    // discovered libraries; previously parsed libs continue to resolve.
    static void setFilter(const std::vector<const char*>& includes,
                          const std::vector<const char*>& excludes);

    // Returns true if a library at the given path should be parsed.
    // jdk_lib_root is a directory prefix derived from libjvm's location; libraries
    // under it are auto-allowed (overridden by an explicit exclude match, with a warning).
    // main_exe is the resolved path of /proc/self/exe (or NULL).
    static bool shouldParseLibrary(const char* path,
                                   const char* jdk_lib_root,
                                   const char* main_exe);
};

class UnloadProtection {
  private:
    void* _lib_handle;
    bool _valid;

  public:
    UnloadProtection(const CodeCache *cc);
    ~UnloadProtection();

    UnloadProtection& operator=(const UnloadProtection& other) = delete;

    bool isValid() const { return _valid; }
};

#endif // _SYMBOLS_H
