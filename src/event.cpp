/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "event.h"
#include "profiler.h"

void EventWithClass::setClassName(const char* class_name, u32 len) {
    _class_name = class_name;
    _class_name_len = len;
}

void EventWithClass::setClassSignature(const char* class_sig) {
    if (class_sig != nullptr) {
        if (class_sig[0] == 'L') {
            setClassName(class_sig + 1, strlen(class_sig) - 2);
        } else {
            setClassName(class_sig, strlen(class_sig));
        }
    }
}

u32 EventWithClass::classId() const {
    if (_class_name == nullptr) {
        return 0;
    }
    return Profiler::instance()->classMap()->lookup(_class_name, _class_name_len);
}
