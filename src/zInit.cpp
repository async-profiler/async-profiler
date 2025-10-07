/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <stdlib.h>
#include <pthread.h>
#include "hooks.h"
#include "profiler.h"
#include "vmStructs.h"

// Invoke functions we may intercept so the GOT entries are resolved
// GOT entries are present in the generated shared library
static void resolveGotEntries() {
    static volatile intptr_t sink;

    void* p0 = malloc(1);
    void* p1 = realloc(p0, 2);
    void* p2 = calloc(1, 1);
    void* p3 = aligned_alloc(1, 1);
    void* p4 = NULL;
    if (posix_memalign(&p4, sizeof(void*), sizeof(void*)) == 0) free(p4);
    free(p3);
    free(p2);
    free(p1);

    sink = (intptr_t)p0 | (intptr_t)p1 | (intptr_t)p2 | (intptr_t)p3 | (intptr_t)p4;

    // This condition will never be true, but the compiler won't optimize it out
    // Here symbols can be called in a safe manner to make sure they're included in the GOT entries of the shared object
    // Any newer symbols needed by the profiler can be called here
    if (!sink) {
        pthread_exit(NULL);
    }
}

// This should be called only after all other statics are initialized.
// Therefore, put it in the last file in the alphabetic order.
class LateInitializer {
  public:
    LateInitializer() {
        Dl_info dl_info;
        if (!OS::isMusl() && dladdr((const void*)Hooks::init, &dl_info) && dl_info.dli_fname != NULL) {
            // Make sure async-profiler DSO cannot be unloaded, since it contains JVM callbacks.
            // This is not relevant for musl, where dlclose() is no-op.
            // Can't use ELF NODELETE flag because of https://sourceware.org/bugzilla/show_bug.cgi?id=20839
            dlopen(dl_info.dli_fname, RTLD_LAZY | RTLD_NODELETE);
        }

        resolveGotEntries();

        if (!checkJvmLoaded()) {
            const char* command = getenv("ASPROF_COMMAND");
            if (command != NULL && OS::checkPreloaded() && Hooks::init()) {
                startProfiler(command);
            }
        }
    }

  private:
    bool checkJvmLoaded() {
        Profiler* profiler = Profiler::instance();
        profiler->updateSymbols(false);

        CodeCache* libjvm = profiler->findLibraryByName(OS::isLinux() ? "libjvm.so" : "libjvm.dylib");
        if (libjvm != NULL && libjvm->findSymbol("AsyncGetCallTrace") != NULL) {
            VMStructs::init(libjvm);
            if (CollectedHeap::created()) {  // heap is already created => this is dynamic attach
                JVMFlag* f = JVMFlag::find("EnableDynamicAgentLoading");
                if (f != NULL && f->isDefault()) {
                    f->setCmdline();
                }
            }
        }

        return libjvm != NULL;
    }

    void startProfiler(const char* command) {
        Error error = _global_args.parse(command);
        _global_args._preloaded = true;

        Log::open(_global_args);

        if (error || (error = Profiler::instance()->run(_global_args))) {
            Log::error("%s", error.message());
        }
    }
};

static LateInitializer _late_initializer;
