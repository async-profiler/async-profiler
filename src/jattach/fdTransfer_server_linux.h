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

#include "../fdTransfer_shared_linux.h"

class FdTransferServer {
  private:
    static int _peer;
    static bool waitForPeer(pid_t peer_pid);
    static bool sendFd(int fd, unsigned int request_id);

  public:
    static bool serveRequests(pid_t pid);
};

#endif // _FDTRANSFER_H
