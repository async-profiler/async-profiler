/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _HTTPCLIENT_H
#define _HTTPCLIENT_H

#include "arguments.h"

class HttpClient {
  public:
    static Error send(const char* url, const char* data, size_t len, Output format);
};

#endif // _HTTPCLIENT_H
