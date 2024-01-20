/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FDTRANSFER_H
#define _FDTRANSFER_H

#ifdef __linux__

#include <linux/perf_event.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof(arr[0]))

#define RESTARTABLE(call)  ({ ssize_t ret; while ((ret = call) < 0 && errno == EINTR); ret; })


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
