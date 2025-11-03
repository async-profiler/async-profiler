/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "engine.h"
#include "log.h"


volatile bool Engine::_enabled = false;

Error Engine::start(Arguments& args) {
    return Error::OK;
}

void Engine::stop() {
}
