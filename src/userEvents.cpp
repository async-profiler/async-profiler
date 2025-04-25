/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "userEvents.h"

Dictionary UserEvents::_dict;

// No (extra) lock is needed here since Dictionary is thread-safe.

int UserEvents::registerEvent(const char* event) {
    return _dict.lookup(event);
}

void UserEvents::collect(std::map<unsigned int, const char*>& map) {
    _dict.collect(map);
}
