/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include "fdtransferServer.h"
#include "../jattach/psutil.h"


int FdTransferServer::_server;
int FdTransferServer::_peer;

bool FdTransferServer::bindServer(struct sockaddr_un *sun, socklen_t addrlen, int accept_timeout) {
    _server = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (_server == -1) {
        perror("FdTransfer socket()");
        return false;
    }

    // Arbitrary timeout, to prevent it from listening forever.
    if (accept_timeout > 0) {
        const struct timeval timeout = {accept_timeout, 0};
        if (setsockopt(_server, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("FdTransfer setsockopt(SO_RCVTIMEO)");
            close(_server);
            return false;
        }
    }

    if (bind(_server, (const struct sockaddr*)sun, addrlen) < 0) {
        perror("FdTransfer bind()");
        close(_server);
        return false;
    }

    if (listen(_server, 1) < 0) {
        perror("FdTransfer listen()");
        close(_server);
        return false;
    }

    return true;
}

bool FdTransferServer::acceptPeer(int *peer_pid) {
    _peer = accept(_server, NULL, NULL);
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

    if (*peer_pid != 0) {
        if (cred.pid != *peer_pid) {
            fprintf(stderr, "Unexpected connection from PID %d, expected from %d\n", cred.pid, *peer_pid);
            close(_peer);
            return false;
        }
    } else {
        *peer_pid = cred.pid;
    }

    return true;
}

bool FdTransferServer::serveRequests(int peer_pid) {
    // Close the server side, don't need it anymore.
    close(_server);

    void *perf_mmap_ringbuf[1024] = {};
    size_t ringbuf_index = 0;
    const size_t perf_mmap_size = 2 * sysconf(_SC_PAGESIZE);

    while (true) {
        unsigned char request_buf[1024];
        struct fd_request *req = (struct fd_request *)request_buf;

        ssize_t ret = RESTARTABLE(recv(_peer, req, sizeof(request_buf), 0));

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

            // In pid == 0 mode, allow all perf_event_open requests.
            // Otherwise, verify the thread belongs to PID.
            if (peer_pid == 0 || syscall(__NR_tgkill, peer_pid, request->tid, 0) == 0) {
                perf_fd = syscall(__NR_perf_event_open, &request->attr, request->tid, -1, -1, 0);
                error = perf_fd < 0 ? errno : 0;
            } else {
                fprintf(stderr, "Target has requested perf_event_open for TID %d which is not a thread of process %d\n", request->tid, peer_pid);
                error = ESRCH;
            }

            // Map the perf buffer here (mapping perf fds may require privileges, and fdtransfer has them while the target application does not
            // necessarily; if pages are already mapped, the same physical pages will be used when the profiler agent maps them again, requiring
            // no privileges this time)
            if (error == 0) {
                // Settings match the mmap() done in PerfEvents::createForThread().
                void *map_result = mmap(NULL, perf_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, perf_fd, 0);
                // Ignore errors - if this fails, let it fail again in the profiler again & produce a proper error for the user.

                // Free next entry in the ring buffer, if it was previously allocated.
                if (perf_mmap_ringbuf[ringbuf_index] != NULL && perf_mmap_ringbuf[ringbuf_index] != MAP_FAILED) {
                    (void)munmap(perf_mmap_ringbuf[ringbuf_index], perf_mmap_size);
                }
                // Store it in the ring buffer so we can free it later.
                perf_mmap_ringbuf[ringbuf_index] = map_result;

                ringbuf_index++;
                ringbuf_index = ringbuf_index % ARRAY_SIZE(perf_mmap_ringbuf);
            }

            struct perf_fd_response resp;
            resp.header.type = request->header.type;
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
            snprintf(tmp_path, sizeof(tmp_path), "/tmp/async-profiler-kallsyms.%d", getpid());

            int kallsyms_fd = -1;
            int error = copyFile("/proc/kallsyms", tmp_path, 0600);
            if (error == 0) {
                kallsyms_fd = open(tmp_path, O_RDONLY);
                if (kallsyms_fd == -1) {
                    error = errno;
                } else {
                    unlink(tmp_path);
                }
            }

            struct fd_response resp;
            resp.type = req->type;
            resp.error = error;
            sendFd(kallsyms_fd, &resp, sizeof(resp));
            close(kallsyms_fd);
            break;
        }

        default:
            fprintf(stderr, "Unknown request type %u\n", req->type);
            break;
        }
    }

    return false;
}

int FdTransferServer::copyFile(const char* src_name, const char* dst_name, mode_t mode) {
    int src = open(src_name, O_RDONLY);
    if (src == -1) {
        return errno;
    }

    int dst = creat(dst_name, mode);
    if (dst == -1) {
        int result = errno;
        close(src);
        return result;
    }

    // copy_file_range() doesn't exist in older kernels, sendfile() no longer works in newer ones
    char buf[65536];
    ssize_t r;
    while ((r = read(src, buf, sizeof(buf))) > 0) {
        ssize_t w = write(dst, buf, r);
        (void)w;
    }

    close(dst);
    close(src);
    return 0;
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

    ssize_t ret = RESTARTABLE(sendmsg(_peer, &msg, 0));
    if (ret < 0) {
        perror("sendmsg()");
        return false;
    }

    return true;
}

bool FdTransferServer::runOnce(int pid, const char *path) {
    // get its nspid prior to moving to its PID namespace.
    int nspid;
    uid_t target_uid;
    gid_t target_gid;
    if (get_process_info(pid, &target_uid, &target_gid, &nspid)) {
        fprintf(stderr, "Process %d not found\n", pid);
        return false;
    }

    struct sockaddr_un sun;
    socklen_t addrlen;
    if (!socketPath(path, &sun, &addrlen)) {
        fprintf(stderr, "Path too long\n");
        return false;
    }

    // Create the server before forking, so we're ready to accept connections once our parent exits.

    // Abstract namespace UDS requires us to move network namespace.
    if (sun.sun_path[0] == '\0') {
        if (enter_ns(pid, "net") == -1) {
            fprintf(stderr, "Failed to enter the net NS of target process %d\n", pid);
            return false;
        }
    }

    if (!bindServer(&sun, addrlen, 30)) {
        return false;
    }

    if (!enter_ns(pid, "pid") == -1) {
        fprintf(stderr, "Failed to enter the PID NS of target process %d\n", pid);
        return false;
    }

    // CLONE_NEWPID affects children only - so we fork here.
    if (fork() == 0) {
        return acceptPeer(&nspid) && serveRequests(nspid);
    } else {
        // Exit now, let our caller continue.
        return true;
    }
}

bool FdTransferServer::runLoop(const char *path) {
    struct sockaddr_un sun;
    socklen_t addrlen;

    struct sigaction sigchld_action;
    sigchld_action.sa_handler = SIG_DFL;
    sigchld_action.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sigchld_action, NULL);

    if (!socketPath(path, &sun, &addrlen)) {
        fprintf(stderr, "Path '%s' is too long\n", path);
        return false;
    }

    if (!FdTransferServer::bindServer(&sun, addrlen, 0)) {
        return false;
    }

    printf("Server ready at '%s'\n", path);

    while (true) {
        int peer_pid = 0;
        if (!acceptPeer(&peer_pid)) {
            return false;
        }

        // Enter its PID namespace.
        if (enter_ns(peer_pid, "pid") == -1) {
            fprintf(stderr, "Failed to enter the PID NS of target process %d\n", peer_pid);
            return false;
        }

        printf("Serving PID %d\n", peer_pid);

        // We fork(), to actually move a PID namespace.
        if (fork() == 0) {
            return FdTransferServer::serveRequests(0);
        } else {
            close(_peer);
        }

        // Move back to our original PID namespace (reverts pid_for_children)
        if (enter_ns(getpid(), "pid") == -1) {
            fprintf(stderr, "Failed to exit the PID NS of target process %d\n", peer_pid);
            return false;
        }
    }
}

#endif // __linux__
