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

#ifndef _FDTRANSFER_SHARED_H
#define _FDTRANSFER_SHARED_H

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
    enum request_type type;
    unsigned int length;
#define MAX_REQUEST_LENGTH 2048
    unsigned int request_id;
};

struct perf_fd_request {
    struct fd_request header;
    pid_t tid;
    struct perf_event_attr attr;
};

struct fd_response {
    // matching request id
    unsigned int response_id;
    // 0 on success, otherwise errno
    int error;
};

struct perf_fd_response {
    struct fd_response header;
    pid_t tid;
};

static inline bool socketPathForPid(pid_t pid, struct sockaddr_un *sun, socklen_t *addrlen) {
    sun->sun_path[0] = '\0';
    const int max_size = sizeof(sun->sun_path) - 1;
    const int path_len = snprintf(sun->sun_path + 1, max_size, "async-profiler-%d", pid);
    if (path_len > max_size) {
        return false;
    }

    sun->sun_family = AF_UNIX;
    // +1 for the first \0 byte
    *addrlen = sizeof(*sun) - (sizeof(sun->sun_path) - (path_len + 1));

    return true;
}

static inline bool socketPath(const char *path, struct sockaddr_un *sun, socklen_t *addrlen) {
    const int path_len = strlen(path);
    if (path_len > sizeof(sun->sun_path) + 1) {
        return false;
    }
    strcpy(sun->sun_path, path);

    sun->sun_family = AF_UNIX;
    *addrlen = sizeof(*sun) - (sizeof(sun->sun_path) - path_len);

    return true;
}

#endif // __linux__

#endif // _FDTRANSFER_SHARED_H
