/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <stdlib.h>
#include "hooks.h"
#include "profiler.h"
#include "symbols.h"
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

        if (!checkJvmLoaded()) {
            const char* command = getenv("ASPROF_COMMAND");
            if (command != NULL && OS::checkPreloaded() && Hooks::init(false)) {
                startProfiler(command);
            }
        }
    }

  private:
    bool checkJvmLoaded() {
        Profiler* profiler = Profiler::instance();
        // checkJvmLoaded only needs libjvm parsed (to find AsyncGetCallTrace
        // and initialize VMStructs). The static-init context here runs while
        // the agent .so is being dlopen()ed, BEFORE any agent callback
        // (Agent_OnLoad / Agent_OnAttach) — so the user's symbols filter
        // hasn't been parsed yet. Doing a full parseLibraries here would
        // bypass that filter and force-load every mapped .so, which on
        // processes with a large number native libraries can spike RSS.
        //
        // Restrict to libjvm here. The full library walk runs later, in
        // VM::init's call to updateSymbols (vmEntry.cpp), by which time
        // Agent_OnLoad/Agent_OnAttach has already invoked Symbols::setFilter
        // with the user's --symbols-include / --symbols-exclude arguments.
        Symbols::setEagerParseLibjvmOnly(true);
        profiler->updateSymbols(false);
        Symbols::setEagerParseLibjvmOnly(false);

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
