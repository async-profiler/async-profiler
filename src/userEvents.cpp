/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "userEvents.h"

Dictionary UserEvents::_dict;
Mutex UserEvents::_state_lock;

int UserEvents::registerEvent(const char *event) {
    MutexLocker locker(_state_lock);
    return _dict.lookup(event);
}

void UserEvents::collect(std::map<unsigned int, const char*>& map) {
    MutexLocker locker(_state_lock);
    _dict.collect(map);
}
