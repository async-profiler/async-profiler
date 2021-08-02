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
#include <sched.h>
#include <fstream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

#include "../fdTransfer_shared_linux.h"
#include "utils.h"


#define TMP_KALLSYMS_PATH "/tmp/async-profiler-kallsyms.%d"

class FdTransferServer {
  private:
    static int _peer;
    static bool waitForPeer(pid_t peer_pid);
    static bool sendFd(int fd, struct fd_response *resp, size_t resp_size);

  public:
    static bool serveRequests(pid_t pid);
};

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

    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(_peer, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1) {
        perror("getsockopt(SO_PEERCRED)");
        close(_peer);
        return false;
    }

    if (cred.pid != peer_pid) {
        fprintf(stderr, "unexpected connection from PID %d, expected from %d\n", cred.pid, peer_pid);
        close(_peer);
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
        struct fd_request *req = (struct fd_request *)request_buf;

        ssize_t ret = recv(_peer, req, sizeof(request_buf), 0);
        if (ret == 0) {
            // EOF means done
            return true;
        } else if (ret < 0) {
            perror("recv()");
            break;
        }

        switch (req->type) {
        case PERF_FD: {
            struct perf_fd_request *request = (struct perf_fd_request*)req;
            int perf_fd = -1;
            int error;

            if (syscall(__NR_tgkill, pid, request->tid, 0) == 0) {
                perf_fd = syscall(__NR_perf_event_open, &request->attr, request->tid, -1, -1, 0);
                if (perf_fd == -1) {
                    error = errno;
                } else {
                    error = 0;
                }
            } else {
                fprintf(stderr, "target has requested perf_event_open for TID %d which is not a thread of process %d\n", request->tid, pid);
                error = ESRCH;
            }

            struct perf_fd_response resp;
            resp.header.response_id = request->header.request_id;
            resp.header.error = error;
            resp.tid = request->tid;
            sendFd(perf_fd, &resp.header, sizeof(resp));
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

            struct fd_response resp;
            resp.response_id = req->request_id;
            int kallsyms_fd = open(tmp_path, O_RDONLY);
            if (kallsyms_fd == -1) {
                resp.error = errno;
            } else {
                unlink(tmp_path);
                resp.error = 0;
            }

            sendFd(kallsyms_fd, &resp, sizeof(resp));
            close(kallsyms_fd);
            break;
        }

        default:
            fprintf(stderr, "unknown request type %u\n", req->type);
            break;
        }
    }

    return false;
}

bool FdTransferServer::sendFd(int fd, struct fd_response *resp, size_t resp_size) {
    struct msghdr msg = {0};

    struct iovec iov[1];
    iov[0].iov_base = resp;
    iov[0].iov_len = resp_size;
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

static bool enter_pid_net_mnt(pid_t pid) {
    // the last one works - because we still have the old /proc accessible, so we see PIDs as they are
    // the NS before we moved.
    return enter_ns(pid, "net") == 1 && enter_ns(pid, "pid") == 1 && enter_ns(pid, "mnt");
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s pid\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);
    if (pid == 0) {
        fprintf(stderr, "Invalid pid: '%s'\n", argv[1]);
        return 1;
    }

    // get its nspid prior to moving to its PID namespace.
    pid_t nspid = -1;
    uid_t _target_uid;
    gid_t _target_gid;
    if (!get_process_info(pid, &_target_uid, &_target_gid, &nspid)) {
        fprintf(stderr, "Process %d not found\n", pid);
        return 1;
    }

    if (nspid < 0) {
        nspid = alt_lookup_nspid(pid);
    }

    if (!enter_pid_net_mnt(pid)) {
        perror("Failed to enter the net NS / PID NS / mount NS of target process");
        return 1;
    }

    // CLONE_NEWPID affects children only - so we fork here.
    if (0 == fork()) {
        _exit(FdTransferServer::serveRequests(nspid) ? 0 : 1);
    } else {
        int status;
        if (-1 == wait(&status)) {
            perror("wait");
            return 1;
        } else {
            bool success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            printf("Done serving%s\n", success ? " successfully" : ", had an error");
            return success ? 0 : 1;
        }
    }
}
