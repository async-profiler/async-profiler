/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "log.h"
#include "symbols.h"

void UnloadProtection::patchImport(ImportId id, void* hook_func) const {
    if (!isValid()) {
        Log::debug("Could not patch '%d' for '%s', unload protection failed", id, _protected_cc->name());
        return;
    }

    if (!_protected_cc->_imports_patchable) {
        _protected_cc->makeImportsPatchable();
    }

    for (int ty = 0; ty < NUM_IMPORT_TYPES; ty++) {
        void** entry = _protected_cc->_imports[id][ty];
        if (entry != NULL) {
            *entry = hook_func;
        }
    }
}
