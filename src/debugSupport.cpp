#include "debugSupport.h"
#include "debug/debug.h"
#include <dlfcn.h>

Shims Shims::_instance;

Shims::Shims() : _tid_setter_ref(NULL) {
    #ifdef DEBUG
    if (_tid_setter_ref == NULL) {
        void* sym_handle = dlsym(RTLD_DEFAULT, "set_sighandler_tid");
        if (_tid_setter_ref != NULL) {
            _tid_setter_ref = (SetSigHandlerTidRef) sym_handle;
        }
    }
    #endif
}

void Shims::setSighandlerTid(int tid) {
    #ifdef DEBUG
    if (_tid_setter_ref != NULL) {
        _tid_setter_ref(tid);
    }
    #endif
}