/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "matcher.h"

Matcher::Matcher(const char* pattern) {
    if (pattern[0] == '*') {
        _type = MATCH_ENDS_WITH;
        _pattern = strdup(pattern + 1);
    } else {
        _type = MATCH_EQUALS;
        _pattern = strdup(pattern);
    }

    _len = strlen(_pattern);
    if (_len > 0 && _pattern[_len - 1] == '*') {
        _type = _type == MATCH_EQUALS ? MATCH_STARTS_WITH : MATCH_CONTAINS;
        _pattern[--_len] = 0;
    }
}

Matcher::~Matcher() {
    free(_pattern);
}

Matcher::Matcher(const Matcher& m) {
    _type = m._type;
    _pattern = strdup(m._pattern);
    _len = m._len;
}

Matcher& Matcher::operator=(const Matcher& m) {
    if (this == &m) return *this;

    free(_pattern);

    _type = m._type;
    _pattern = strdup(m._pattern);
    _len = m._len;

    return *this;
}

bool Matcher::matches(const char* s) const {
    switch (_type) {
        case MATCH_EQUALS:
            return strcmp(s, _pattern) == 0;
        case MATCH_CONTAINS:
            return strstr(s, _pattern) != NULL;
        case MATCH_STARTS_WITH:
            return strncmp(s, _pattern, _len) == 0;
        case MATCH_ENDS_WITH:
            int slen = strlen(s);
            return slen >= _len && strcmp(s + slen - _len, _pattern) == 0;
    }
    return false;
}
