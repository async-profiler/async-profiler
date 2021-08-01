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

#include "fdTransfer_server_linux.h"
#include "utils.h"


#define TMP_KALLSYMS_PATH "/tmp/async-profiler-kallsyms.%d"

int FdTransferServer::_peer;

bool FdTransferServer::waitForPeer(pid_t peer_pid) {
    int listener = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listener == -1) {
        perror("FdTransfer socket():");
        return false;
    }

    struct sockaddr_un sun;
    socklen_t addrlen;
    socketPathForPid(peer_pid, &sun, &addrlen);

    if (-1 == bind(listener, (const struct sockaddr*)&sun, addrlen)) {
        perror("FdTransfer bind()");
        close(listener);
        return false;
    }

    if (-1 == listen(listener, 1)) {
        perror("FdTransfer listen()");
        close(listener);
        return false;
    }

    _peer = accept(listener, NULL, NULL);
    close(listener);
    if (_peer == -1) {
        perror("FdTransfer accept()");
        return false;
    }

    return true;
}

bool FdTransferServer::serveRequests(pid_t pid) {
    if (!waitForPeer(pid)) {
        return false;
    }

    while (1) {
        unsigned char request_buf[MAX_REQUEST_LENGTH];
        struct fd_request *header = (struct fd_request *)request_buf;

        ssize_t ret = recv(_peer, header, sizeof(request_buf), 0);
        if (ret == 0) {
            // EOF means done
            return true;
        } else if (ret < 0) {
            perror("recv()");
            break;
        }

        switch (header->type) {
        case PERF_FD: {
            struct perf_fd_request *request = (struct perf_fd_request*)header;
            int perf_fd = -1;

            if (syscall(__NR_tgkill, pid, request->tid, 0) == 0) {
                perf_fd = syscall(__NR_perf_event_open, &request->attr, request->tid, -1, -1, 0);
                if (perf_fd == -1) {
                    perror("perf_event_open()");
                }
            } else {
                fprintf(stderr, "target has requested perf_event_open for TID %d which is not a thread of process %d\n", request->tid, pid);
            }

            sendFd(perf_fd, request->header.request_id);
            close(perf_fd);
            break;
        }

        case KALLSYMS_FD: {
            // can't directly pass the fd of /proc/kallsyms, because before Linux 4.15 the permission check
            // was conducted on each read.
            // it's simpler to copy the file to a temporary location and pass the fd of it (compared to passing the
            // entire contents over the peer socket)
            char tmp_path[256];
            snprintf(tmp_path, sizeof(tmp_path), TMP_KALLSYMS_PATH, getpid());
            {
                // TODO from Linux 3.15, we can use memfds here. Also, from 3.11 we can use O_TMPFILE.
                std::ifstream src("/proc/kallsyms", std::ios::binary);
                close(creat(tmp_path, S_IRUSR | S_IWUSR));  // create it with 0600 so others can't read.
                std::ofstream dst(tmp_path, std::ios::binary);
                dst << src.rdbuf();
            }

            int kallsyms_fd = open(tmp_path, O_RDONLY);
            if (kallsyms_fd == -1) {
                perror("open() tmp kallsyms");
                kallsyms_fd = open("/dev/null", O_RDONLY);
            } else {
                unlink(tmp_path);
            }

            sendFd(kallsyms_fd, header->request_id);
            close(kallsyms_fd);
            break;
        }

        default:
            fprintf(stderr, "unknown request type %u\n", header->type);
            break;
        }
    }

    return false;
}

bool FdTransferServer::sendFd(int fd, unsigned int request_id) {
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

    if (fd != -1) {
        msg.msg_control = u.buf;
        msg.msg_controllen = sizeof(u.buf);

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
        memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
    }

    ssize_t ret = sendmsg(_peer, &msg, 0);
    if (ret < 0) {
        perror("sendmsg()");
        return false;
    }

    return true;
}

#endif // __linux__
