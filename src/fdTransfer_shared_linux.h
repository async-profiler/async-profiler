/*
 * Copyright 2021 Yonatan Goldschmidt
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

static inline void socketPathForPid(pid_t pid, struct sockaddr_un *sun, socklen_t *addrlen) {
    char path[sizeof(sun->sun_path)];

    path[0] = '\0';
    snprintf(path + 1, sizeof(path) - 1, "async-profiler-%d", pid);

    sun->sun_family = AF_UNIX;
    memcpy(sun->sun_path, path, sizeof(sun->sun_path));

    *addrlen = sizeof(*sun) - (sizeof(sun->sun_path) - (strlen(path + 1) + 1));
}

#endif // __linux__

#endif // _FDTRANSFER_SHARED_H
