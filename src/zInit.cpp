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

        if (!checkJvmLoaded() && checkPreload()) {
            const char* command = getenv("ASPROF_COMMAND");
            if (command != NULL && Hooks::init(false)) {
                startProfiler(command);
            }
        }
    }

private:
    // Function checks if the current async-profiler shared object is preloaded or not
    // This is done by checking which shared object the dlopen belongs to
    // If that shared object is the same as the current shared object that would mean that the profiler is in PRELOAD mode
    // However if the dlopen belongs to a different shared object that would indicate that the profiler started in a different mode
    static bool checkPreload(){
        Dl_info current_info;
        if (dladdr((const void*)Hooks::init, &current_info) == 0 || current_info.dli_fname == NULL) {
            return false;
        }

        Dl_info dlopen_info;
        if (dladdr((const void*)dlopen, &dlopen_info) == 0 || dlopen_info.dli_fname == NULL) {
            return false;
        }

        return strcmp(dlopen_info.dli_fname, current_info.dli_fname) == 0;
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
