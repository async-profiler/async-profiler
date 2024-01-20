/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FDTRANSFER_CLIENT_H
#define _FDTRANSFER_CLIENT_H

#ifdef __linux__

#include "fdtransfer.h"

class FdTransferClient {
  private:
    static int _peer;

    static int recvFd(unsigned int request_id, struct fd_response *resp, size_t resp_size);

  public:
    static bool connectToServer(const char *path);
    static bool hasPeer() { return _peer != -1; }
    static void closePeer() {
        if (_peer != -1) {
            close(_peer);
            _peer = -1;
        }
    }

    static int requestPerfFd(int *tid, struct perf_event_attr *attr);
    static int requestKallsymsFd();
};

#else

class FdTransferClient {
  public:
    static bool connectToServer(const char *path) { return false; }
    static bool hasPeer() { return false; }
    static void closePeer() { }
};

#endif // __linux__

#endif // _FDTRANSFER_CLIENT_H
