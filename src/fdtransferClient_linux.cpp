/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "fdtransferClient.h"
#include "log.h"


int FdTransferClient::_peer = -1;

bool FdTransferClient::connectToServer(const char *path) {
    closePeer();

    _peer = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (_peer == -1) {
        Log::warn("FdTransferClient socket(): %s", strerror(errno));
        return false;
    }

    struct sockaddr_un sun;
    socklen_t addrlen;
    if (!socketPath(path, &sun, &addrlen)) {
        return false;
    }

    // Do not block for more than 10 seconds when waiting for a response
    struct timeval tv = {10, 0};
    setsockopt(_peer, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(_peer, (const struct sockaddr *)&sun, addrlen) == -1) {
        Log::warn("FdTransferClient connect(): %s", strerror(errno));
        return false;
    }

    return true;
}

int FdTransferClient::requestPerfFd(int* tid, int target_cpu, struct perf_event_attr* attr, const char* probe_name) {
    struct perf_fd_request request;
    request.header.type = PERF_FD;
    request.tid = *tid;
    request.target_cpu = target_cpu;
    memcpy(&request.attr, attr, sizeof(request.attr));
    *stpncpy(request.probe_name, probe_name, sizeof(request.probe_name) - 1) = 0;

    size_t request_size = sizeof(request) - sizeof(request.probe_name) + strlen(request.probe_name) + 1;
    if (RESTARTABLE(send(_peer, &request, request_size, 0)) != request_size) {
        Log::warn("FdTransferClient send(): %s", strerror(errno));
        return -1;
    }

    struct perf_fd_response resp;
    int fd = recvFd(request.header.type, &resp.header, sizeof(resp));
    if (fd == -1) {
        // Update errno for our caller.
        errno = resp.header.error;
    } else {
        // Update the TID of createForThread, in case the multiple threads' requests got mixed up and we're
        // now handling the response destined to another. It's alright - the other thread(s) will finish the
        // handling of our TID perf fd.
        *tid = resp.tid;
    }
    return fd;
}

int FdTransferClient::requestKallsymsFd() {
    struct fd_request request;
    request.type = KALLSYMS_FD;

    if (RESTARTABLE(send(_peer, &request, sizeof(request), 0)) != sizeof(request)) {
        Log::warn("FdTransferClient send(): %s", strerror(errno));
        return -1;
    }

    struct fd_response resp;
    int fd = recvFd(request.type, &resp, sizeof(resp));
    if (fd == -1) {
        errno = resp.error;
    }

    return fd;
}

int FdTransferClient::recvFd(unsigned int type, struct fd_response *resp, size_t resp_size) {
    struct msghdr msg = {0};

    struct iovec iov[1];
    iov[0].iov_base = resp;
    iov[0].iov_len = resp_size;
    msg.msg_iov = iov;
    msg.msg_iovlen = ARRAY_SIZE(iov);

    int newfd;
    union {
        char buf[CMSG_SPACE(sizeof(newfd))];
        struct cmsghdr align;
    } u;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    ssize_t ret = RESTARTABLE(recvmsg(_peer, &msg, 0));
    if (ret < 0) {
        Log::warn("FdTransferClient recvmsg(): %s", strerror(errno));
        return -1;
    }

    if (resp->type != type) {
        Log::warn("FdTransferClient recvmsg(): bad response type");
        return -1;
    }

    if (resp->error == 0) {
        struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
        if (cmptr != NULL && cmptr->cmsg_len == CMSG_LEN(sizeof(newfd))
            && cmptr->cmsg_level == SOL_SOCKET && cmptr->cmsg_type == SCM_RIGHTS) {

            newfd = *((int*)CMSG_DATA(cmptr));
        } else {
            Log::warn("FdTransferClient recvmsg(): unexpected response with no SCM_RIGHTS: %s", strerror(errno));
            newfd = -1;
        }
    } else {
        newfd = -1;
    }

    return newfd;
}

#endif // __linux__
