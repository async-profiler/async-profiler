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

#ifndef _FDTRANSFER_H
#define _FDTRANSFER_H

#include <linux/perf_event.h>


class FdTransfer {
  private:
    static const int _connect_retries = 50;
    static const int _connect_retry_interval_ms = 100;
    static const int _accept_timeout_ms = 10000;
    static int _listener; // if not -1, we're in listen mode.
    static int _peer;
    static unsigned int _request_id;

    static void socketPathForPid(pid_t pid, struct sockaddr_un *sun, socklen_t *addrlen);
    static unsigned int nextRequestId() { return _request_id++; }
    static int recvFd(unsigned int request_id);
    static bool sendFd(int fd, unsigned int request_id);

  public:
    static bool connectToTarget(pid_t pid);
    static bool initializeListener();
    static bool isListenerInitialized() { return _listener != -1; }
    static bool acceptPeer();
    static bool hasPeer() { return _peer != -1; }
    static void closePeer() {
        if (_peer != -1) {
            close(_peer);
            _peer = -1;
        }
    }

    static bool serveRequests();

    static int requestPerfFd(pid_t tid, struct perf_event_attr *attr);
    static int requestKallsymsFd();
};

#endif // _FDTRANSFER_H
