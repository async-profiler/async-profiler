/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include "codeCache.h"
#include "profiler.h"
#include "testRunner.hpp"
#include <dlfcn.h>

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
    void* result = dlopen("libvaddrdif.so", RTLD_NOW);
    if (!result) {
        printf("%s\n", dlerror());
    }
    ASSERT(result);
    Profiler::instance()->updateSymbols(false);

    PerfEventType* event_type = PerfEventType::forName("vaddrdif_square");
    ASSERT(event_type);

    int (*square)(int) = (int (*)(int))event_type->config1;
    ASSERT_EQ(square(5), 25);
}

#endif // __linux__
