/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <stdlib.h>
#include "hooks.h"
#include "profiler.h"
#include "vmStructs.h"


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

        if (!checkJvmLoaded() && checkPreloaded()) {
            const char* command = getenv("ASPROF_COMMAND");
            if (command != NULL && Hooks::init()) {
                startProfiler(command);
            }
        }
    }

  private:
    static bool checkPreloaded() {
        Dl_info current_info;
        if (dladdr((const void*)Hooks::init, &current_info) == 0 || current_info.dli_fname == NULL) {
            return false;
        }

        Dl_info sigaction_info;
#ifdef __linux__
        // On Linux: Check if sigaction belong to the profiler shared objects
        // If the sigaction is found inside the profiler shared objects that is a good indication that the profiler is preloaded
        if (dladdr((const void*)sigaction, &sigaction_info) == 0 || sigaction_info.dli_fname == NULL) {
            return false;
        }
#else
        // On macOS: We can't check for sigaction directly via dladdr as it will still refer to the original implementation,
        // this is due to how interposing of symbols works on macOS when using the DYLD_INSERT_LIBRARIES env variable.
        // To bypass this we use dlsym to find the reference of sigaction for other shared objects loaded into the memory,
        // If this sigaction belongs to the async-profiler shared objects then that's a good indication that the profiler is preloaded
        void* sigaction_reference = dlsym(RTLD_NEXT, "sigaction");
        if (sigaction_reference == NULL || dladdr((const void*)sigaction_reference, &sigaction_info) == 0 || sigaction_info.dli_fname == NULL) {
            return false;
        }
#endif
        return strcmp(sigaction_info.dli_fname, current_info.dli_fname) == 0;
    }

    static bool checkJvmLoaded() {
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
