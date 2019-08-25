/*
 * Copyright 2018 Andrei Pangin
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

#include <arpa/inet.h>
#include <byteswap.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "os.h"


class LinuxThreadList : public ThreadList {
  private:
    DIR* _dir;

  public:
    LinuxThreadList() {
        _dir = opendir("/proc/self/task");
    }

    ~LinuxThreadList() {
        if (_dir != NULL) {
            closedir(_dir);
        }
    }

    int next() {
        if (_dir != NULL) {
            struct dirent* entry;
            while ((entry = readdir(_dir)) != NULL) {
                if (entry->d_name[0] != '.') {
                    return atoi(entry->d_name);
                }
            }
        }
        return -1;
    }
};


u64 OS::nanotime() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (u64)tp.tv_sec * 1000000000 + tp.tv_nsec;
}

u64 OS::millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

u64 OS::hton64(u64 x) {
    return htonl(1) == 1 ? x : bswap_64(x);
}

int OS::threadId() {
    return syscall(__NR_gettid);
}

bool OS::isThreadRunning(int thread_id) {
    char buf[512];
    sprintf(buf, "/proc/self/task/%d/stat", thread_id);
    int fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    bool running = false;
    if (read(fd, buf, sizeof(buf)) > 0) {
        char* s = strchr(buf, ')');
        running = s != NULL && (s[2] == 'R' || s[2] == 'D');
    }

    close(fd);
    return running;
}

bool OS::isSignalSafeTLS() {
    return true;
}

bool OS::isJavaLibraryVisible() {
    return false;
}

void OS::installSignalHandler(int signo, void (*handler)(int, siginfo_t*, void*)) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;

    sigaction(signo, &sa, NULL);
}

void OS::sendSignalToThread(int thread_id, int signo) {
    static const int self_pid = getpid();

    syscall(__NR_tgkill, self_pid, thread_id, signo);
}

ThreadList* OS::listThreads() {
    return new LinuxThreadList();
}

#endif // __linux__
