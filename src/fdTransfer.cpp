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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/syscall.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <stdbool.h>
#include <sched.h>
#include <fstream>

#include "fdTransfer.h"
#include "log.h"

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

#define TMP_KALLSYMS_PATH "/tmp/async-profiler-kallsyms"

int FdTransfer::_listener = -1;
int FdTransfer::_peer = -1;
unsigned int FdTransfer::_request_id = 0;

// this function uses perror & fprintf instead of the Log class, because it doesn't execute
// as part of async-profiler (but instead, in fdtransfer).
bool FdTransfer::serveRequests() {
    while (1) {
        unsigned char request_buf[MAX_REQUEST_LENGTH];
        struct fd_request *header = (struct fd_request *)request_buf;

        ssize_t ret = recv(_peer, header, sizeof(*header), 0);
        if (ret < 0) {
            perror("recv()");
            break;
        } else if (ret == 0) {
            // EOF means done
            return true;
        } else if (ret != sizeof(*header)) {
            // TODO truncated reads
            fprintf(stderr, "truncated request header\n");
            break;
        }

        if (header->length > MAX_REQUEST_LENGTH) {
            fprintf(stderr, "too large request: %zd\n", ret);
            break;
        } else if (header->length < sizeof(*header)) {
            fprintf(stderr, "bogus header length\n");
            break;
        }

        const size_t left = header->length - sizeof(*header);
        if (left > 0) {
            ret = recv(_peer, &header[1], header->length, 0);
            if (ret < 0) {
                perror("recv()");
                break;
            } else if (ret != left) {
                // TODO truncated reads
                fprintf(stderr, "truncated request\n");
                break;
            }
        }

        switch (header->type) {
        case PERF_FD: {
            // TODO validate 'tid' is indeed a thread of 'nspid'
            struct perf_fd_request *request = (struct perf_fd_request*)header;

            int perf_fd = syscall(__NR_perf_event_open, &request->attr, request->tid, -1, -1, 0);
            if (perf_fd == -1) {
                perror("perf_event_open()");
            }

            sendFd(perf_fd, request->header.request_id);
            break;
        }

        case KALLSYMS_FD: {
            // can't directly pass the fd of /proc/kallsyms, because before Linux 4.15 the permission check
            // was conducted on each read.
            // it's simpler to copy the file to a temporary location and pass the fd of it (compared to passing the
            // entire contents over the peer socket)
            {
                // TODO from Linux 3.15, we can use memfds here
                std::ifstream src("/proc/kallsyms", std::ios::binary);
                // TODO this creates the file with 0611, it should be 0600.
                std::ofstream dst(TMP_KALLSYMS_PATH, std::ios::binary);
                dst << src.rdbuf();
            }

            int kallsyms_fd = open(TMP_KALLSYMS_PATH, O_RDONLY);
            if (kallsyms_fd == -1) {
                perror("open() tmp kallsyms");
            }

            sendFd(kallsyms_fd, header->request_id);
            break;
        }

        default:
            fprintf(stderr, "unknown request type %u\n", header->type);
            break;
        }
    }

    return false;
}

// create the UDS abstract domain socket path for "pid".
void FdTransfer::socketPathForPid(pid_t pid, struct sockaddr_un *sun, socklen_t *addrlen) {
    char path[sizeof(sun->sun_path)];

    path[0] = '\0';
    snprintf(path + 1, sizeof(path) - 1, "async-profiler-%d", pid);

    sun->sun_family = AF_UNIX;
    memcpy(sun->sun_path, path, sizeof(sun->sun_path));

    *addrlen = sizeof(*sun) - (sizeof(sun->sun_path) - (strlen(path + 1) + 1));
}

// about perror usage, see comment on serveRequests
bool FdTransfer::connectToTarget(pid_t pid) {
    _peer = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_peer == -1) {
        perror("socket()");
        return false;
    }

    struct sockaddr_un sun;
    socklen_t addrlen;
    socketPathForPid(pid, &sun, &addrlen);

    for (int i = 0; i < _connect_retries; i++) {
        if (0 == connect(_peer, (const struct sockaddr *)&sun, addrlen)) {
            return true;
        }

        if (errno == ECONNREFUSED) {
            const struct timespec ts = {
                _connect_retry_interval_ms / 1000,
                (_connect_retry_interval_ms % 1000) * 1000000,
            };
            nanosleep(&ts, NULL);
        } else {
            perror("connect()");
            return false;
        }
    }

    // retries are out.
    return false;
}

bool FdTransfer::initializeListener() {
    _listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_listener == -1) {
        Log::error("FdTransfer socket(): %s", strerror(errno));
        return false;
    }

    struct sockaddr_un sun;
    socklen_t addrlen;
    socketPathForPid(getpid(), &sun, &addrlen);

    if (-1 == bind(_listener, (const struct sockaddr*)&sun, addrlen)) {
        Log::error("FdTransfer bind(): %s", strerror(errno));
        return false;
    }

    const struct timeval timeout = {
        _accept_timeout_ms / 1000,
        (_accept_timeout_ms % 1000) * 1000,
    };
    if (-1 == setsockopt(_listener, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
        Log::error("FdTransfer setsockopt(SO_RCVTIMEO): %s", strerror(errno));
        return false;
    }

    if (-1 == listen(_listener, 1)) {
        Log::error("FdTransfer listen(): %s", strerror(errno));
        return true;
    }

    return true;
}

bool FdTransfer::acceptPeer() {
    _peer = accept(_listener, NULL, NULL);
    if (_peer == -1) {
        Log::error("FdTransfer accept(): %s", strerror(errno));
        return false;
    }

    return true;
}

int FdTransfer::requestPerfFd(pid_t tid, struct perf_event_attr *attr) {
    struct perf_fd_request request;

    request.header.type = PERF_FD;
    request.header.length = sizeof(request);
    request.header.request_id = nextRequestId();

    request.tid = tid;
    memcpy(&request.attr, attr, sizeof(request.attr));

    if (send(_peer, &request, sizeof(request), 0) != sizeof(request)) {
        return -1;
    }

    return recvFd(request.header.request_id);
}

int FdTransfer::requestKallsymsFd() {
    struct fd_request request;

    request.type = KALLSYMS_FD;
    request.length = sizeof(request);
    request.request_id = nextRequestId();

    if (send(_peer, &request, sizeof(request), 0) != sizeof(request)) {
        return -1;
    }

    return recvFd(request.request_id);
}

// about perror usage, see comment on serveRequests
bool FdTransfer::sendFd(int fd, unsigned int request_id) {
    struct msghdr msg = {0};

    struct iovec iov[1] = {
        {
            &request_id,
            sizeof(request_id),
        },
    };
    msg.msg_iov = iov;
    msg.msg_iovlen = ARRAY_SIZE(iov);

    union {
       char buf[CMSG_SPACE(sizeof(fd))];
       struct cmsghdr align;
    } u;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

    if (sizeof(request_id) != sendmsg(_peer, &msg, 0)) {
        // TODO truncated writes
        perror("sendmsg()");
        return false;
    }

    return true;
}

int FdTransfer::recvFd(unsigned int request_id) {
    struct msghdr msg = {0};

    struct iovec iov[1];
    int recv_request_id;
    iov[0].iov_base = &recv_request_id;
    iov[0].iov_len = sizeof(recv_request_id);
    msg.msg_iov = iov;
    msg.msg_iovlen = ARRAY_SIZE(iov);

    int newfd;
    union {
        char buf[CMSG_SPACE(sizeof(newfd))];
        struct cmsghdr align;
    } u;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    ssize_t ret = recvmsg(_peer, &msg, 0);
    if (ret < 0) {
        Log::warn("FdTransfer recvmsg(): %s", strerror(errno));
        return -1;
    }

    if (ret != sizeof(recv_request_id) || recv_request_id != request_id) {
        // TODO truncated reads
        Log::warn("FdTransfer recvmsg(): truncated read", strerror(errno));
        return -1;
    }

    struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
    if (cmptr != NULL && cmptr->cmsg_len == CMSG_LEN(sizeof(newfd))
        && cmptr->cmsg_level == SOL_SOCKET && cmptr->cmsg_type == SCM_RIGHTS) {

        newfd = *((typeof(newfd) *)CMSG_DATA(cmptr));
    } else {
        Log::warn("FdTransfer recvmsg(): unexpected response with no SCM_RIGHTS", strerror(errno));
        newfd = -1;
    }

    return newfd;
}
