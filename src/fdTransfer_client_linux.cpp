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

#ifdef __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
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

#include "fdTransfer_client.h"
#include "log.h"


int FdTransferClient::_peer = -1;
unsigned int FdTransferClient::_request_id = 0;

bool FdTransferClient::connectToServer(pid_t pid) {
    _peer = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (_peer == -1) {
        Log::warn("FdTransferClient socket(): %s", strerror(errno));
        return false;
    }

    struct sockaddr_un sun;
    socklen_t addrlen;
    socketPathForPid(pid, &sun, &addrlen);

    if (connect(_peer, (const struct sockaddr *)&sun, addrlen) == -1) {
        Log::warn("FdTransferClient connect(): %s", strerror(errno));
        return false;
    }

    return true;
}

int FdTransferClient::requestPerfFd(pid_t tid, struct perf_event_attr *attr) {
    struct perf_fd_request request;

    request.header.type = PERF_FD;
    request.header.length = sizeof(request);
    request.header.request_id = nextRequestId();

    request.tid = tid;
    memcpy(&request.attr, attr, sizeof(request.attr));

    if (send(_peer, &request, sizeof(request), 0) != sizeof(request)) {
        Log::warn("FdTransferClient send(): %s", strerror(errno));
        return -1;
    }

    return recvFd(request.header.request_id);
}

int FdTransferClient::requestKallsymsFd() {
    struct fd_request request;

    request.type = KALLSYMS_FD;
    request.length = sizeof(request);
    request.request_id = nextRequestId();

    if (send(_peer, &request, sizeof(request), 0) != sizeof(request)) {
        Log::warn("FdTransferClient send(): %s", strerror(errno));
        return -1;
    }

    return recvFd(request.request_id);
}

int FdTransferClient::recvFd(unsigned int request_id) {
    struct msghdr msg = {0};

    struct iovec iov[1];
    unsigned int recv_request_id;
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
        Log::warn("FdTransferClient recvmsg(): %s", strerror(errno));
        return -1;
    }

    if (recv_request_id != request_id) {
        Log::warn("FdTransferClient recvmsg(): bad response ID");
        return -1;
    }

    struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
    if (cmptr != NULL && cmptr->cmsg_len == CMSG_LEN(sizeof(newfd))
        && cmptr->cmsg_level == SOL_SOCKET && cmptr->cmsg_type == SCM_RIGHTS) {

        newfd = *((typeof(newfd) *)CMSG_DATA(cmptr));
    } else {
        Log::warn("FdTransferClient recvmsg(): unexpected response with no SCM_RIGHTS: %s", strerror(errno));
        newfd = -1;
    }

    return newfd;
}

#endif // __linux__
