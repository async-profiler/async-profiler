/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FDTRANSFER_SERVER_H
#define _FDTRANSFER_SERVER_H

#ifdef __linux__

#include "../fdtransfer.h"

class FdTransferServer {
  private:
    static int _server;
    static int _peer;

    static int copyFile(const char* src_name, const char* dst_name, mode_t mode);
    static bool sendFd(int fd, struct fd_response *resp, size_t resp_size);

    static bool bindServer(struct sockaddr_un *sun, socklen_t addrlen, int accept_timeout);
    static bool acceptPeer(int *peer_pid);
    static bool serveRequests(int peer_pid);

  public:
    static bool supported() { return true; }

    static bool runOnce(int pid, const char *path);
    static bool runLoop(const char *path);
};

#else

class FdTransferServer {
  public:
    static bool supported() { return false; }

    static bool runOnce(int pid, const char *path) { return false; }
    static bool runLoop(const char *path) { return false; }
};

#endif // __linux__

#endif // _FDTRANSFER_SERVER_H
