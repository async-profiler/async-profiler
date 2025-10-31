/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "engine.h"
#include "log.h"


volatile bool Engine::_enabled = false;

Error Engine::check(Arguments& args) {
    Log::warn("DEPRECATED: The 'check' command is deprecated and will be removed in the next release.");
    return Error::OK;
}

Error Engine::start(Arguments& args) {
    return Error::OK;
}

void Engine::stop() {
}
