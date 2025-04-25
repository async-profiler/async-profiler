/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include "codeCache.h"
#include "profiler.h"
#include "testRunner.hpp"
#include <dlfcn.h>

const void* resolveSymbol(const char* lib, const char* name) {
    void* result = dlopen(lib, RTLD_NOW);
    if (!result) {
        printf("%s\n", dlerror());
        return nullptr;
    }
    Profiler::instance()->updateSymbols(false);
    return Profiler::instance()->resolveSymbol(name);
}

#define ASSERT_RESOLVE(id)                                                             \
    {                                                                                  \
        void* result = dlopen("libreladyn.so", RTLD_NOW); /* see reladyn.c */          \
        ASSERT(result);                                                                \
        Profiler::instance()->updateSymbols(false);                                    \
        CodeCache* libreladyn = Profiler::instance()->findLibraryByName("libreladyn"); \
        ASSERT(libreladyn);                                                            \
        void* sym = libreladyn->findImport(id);                                        \
        ASSERT(sym);                                                                   \
    }

TEST_CASE(ResolveFromRela_plt) {
    ASSERT_RESOLVE(im_pthread_create);
}

TEST_CASE(ResolveFromRela_dyn_R_GLOB_DAT) {
    ASSERT_RESOLVE(im_pthread_setspecific);
}

TEST_CASE(ResolveFromRela_dyn_R_ABS64) {
    ASSERT_RESOLVE(im_pthread_exit);
}

TEST_CASE(VirtAddrDifferentLoadAddr) {
    const void* sym = resolveSymbol("libvaddrdif.so", "vaddrdif_square");
    ASSERT(sym);

    int (*square)(int) = (int (*)(int))sym;
    ASSERT_EQ(square(5), 25);
}

TEST_CASE(MappedTwiceAtZeroOffset) {
    const void* sym = resolveSymbol("libtwiceatzero.so", "twiceatzero_hello");
    // Resolving the symbol without crashing is enough for this test case.
    ASSERT(sym);

    void (*hello)() = (void (*)())sym;
    hello();
}

TEST_CASE(MultipleMatchingSymbols) {
    const void* sym = resolveSymbol("multiplematching.so", "Class::function");
    ASSERT(sym);

    const void* sym_ok = resolveSymbol("multiplematching.so", "_ZN5Class8functionEv");
    ASSERT_EQ(sym, sym_ok);

    const void* sym_cold = resolveSymbol("multiplematching.so", "_ZN5Class8functionEv.cold");
    ASSERT_NE(sym, sym_cold);
}

#endif // __linux__
