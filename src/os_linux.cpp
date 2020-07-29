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
    int _thread_count;

    int getThreadCount() {
        char buf[512];
        int fd = open("/proc/self/stat", O_RDONLY);
        if (fd == -1) {
            return 0;
        }

        int thread_count = 0;
        if (read(fd, buf, sizeof(buf)) > 0) {
            char* s = strchr(buf, ')');
            if (s != NULL) {
                // Read 18th integer field after the command name
                for (int field = 0; *s != ' ' || ++field < 18; s++) ;
                thread_count = atoi(s + 1);
            }
        }

        close(fd);
        return thread_count;
    }

  public:
    LinuxThreadList() {
        _dir = opendir("/proc/self/task");
        _thread_count = -1;
    }

    ~LinuxThreadList() {
        if (_dir != NULL) {
            closedir(_dir);
        }
    }

    void rewind() {
        if (_dir != NULL) {
            rewinddir(_dir);
        }
        _thread_count = -1;
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

    int size() {
        if (_thread_count < 0) {
            _thread_count = getThreadCount();
        }
        return _thread_count;
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

u64 OS::ntoh64(u64 x) {
    return ntohl(1) == 1 ? x : bswap_64(x);
}

int OS::getMaxThreadId() {
    char buf[16] = "65536";
    int fd = open("/proc/sys/kernel/pid_max", O_RDONLY);
    if (fd != -1) {
        ssize_t r = read(fd, buf, sizeof(buf) - 1);
        (void) r;
        close(fd);
    }
    return atoi(buf);
}

int OS::threadId() {
    return syscall(__NR_gettid);
}

bool OS::threadName(int thread_id, char* name_buf, size_t name_len) {
    char buf[64];
    sprintf(buf, "/proc/self/task/%d/comm", thread_id);
    int fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    ssize_t r = read(fd, name_buf, name_len);
    close(fd);

    if (r > 0) {
        name_buf[r - 1] = 0;
        return true;
    }
    return false;
}

ThreadState OS::threadState(int thread_id) {
    char buf[512];
    sprintf(buf, "/proc/self/task/%d/stat", thread_id);
    int fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return THREAD_INVALID;
    }

    ThreadState state = THREAD_INVALID;
    if (read(fd, buf, sizeof(buf)) > 0) {
        char* s = strchr(buf, ')');
        state = s != NULL && (s[2] == 'R' || s[2] == 'D') ? THREAD_RUNNING : THREAD_SLEEPING;
    }

    close(fd);
    return state;
}

ThreadList* OS::listThreads() {
    return new LinuxThreadList();
}

bool OS::isJavaLibraryVisible() {
    return false;
}

void OS::installSignalHandler(int signo, SigAction action, SigHandler handler) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);

    if (handler != NULL) {
        sa.sa_handler = handler;
        sa.sa_flags = 0;
    } else {
        sa.sa_sigaction = action;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
    }

    sigaction(signo, &sa, NULL);
}

bool OS::sendSignalToThread(int thread_id, int signo) {
    static const int self_pid = getpid();

    return syscall(__NR_tgkill, self_pid, thread_id, signo) == 0;
}

#endif // __linux__
