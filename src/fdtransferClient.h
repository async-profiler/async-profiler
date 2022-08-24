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

#ifndef _FDTRANSFER_CLIENT_H
#define _FDTRANSFER_CLIENT_H

#ifdef __linux__

#include "fdtransfer/fdtransfer.h"

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
