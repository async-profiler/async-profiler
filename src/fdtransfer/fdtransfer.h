/*
 * Copyright 2021 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _FDTRANSFER_H
#define _FDTRANSFER_H

#ifdef __linux__

#include <linux/perf_event.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof(arr[0]))


// base header for all requests
enum request_type {
    PERF_FD,
    KALLSYMS_FD,
};

struct fd_request {
    // of type "enum request_type"
    unsigned int type;
};

struct perf_fd_request {
    struct fd_request header;
    int tid;
    struct perf_event_attr attr;
};

struct fd_response {
    // of type "enum request_type"
    unsigned int type;
    // 0 on success, otherwise errno
    int error;
};

struct perf_fd_response {
    struct fd_response header;
    int tid;
};

static inline bool socketPath(const char *path, struct sockaddr_un *sun, socklen_t *addrlen) {
    const int path_len = strlen(path);
    if (path_len > sizeof(sun->sun_path)) {
        return false;
    }
    memcpy(sun->sun_path, path, path_len);
    if (sun->sun_path[0] == '@') {
        sun->sun_path[0] = '\0';
    }

    sun->sun_family = AF_UNIX;
    *addrlen = sizeof(*sun) - (sizeof(sun->sun_path) - path_len);

    return true;
}

#endif // __linux__

#endif // _FDTRANSFER_H
