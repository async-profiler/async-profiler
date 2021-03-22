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
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "os.h"


#ifdef __LP64__
#  define MMAP_SYSCALL __NR_mmap
#else
#  define MMAP_SYSCALL __NR_mmap2
#endif


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


JitWriteProtection::JitWriteProtection(bool enable) {
    // Not used on Linux
}

JitWriteProtection::~JitWriteProtection() {
    // Not used on Linux
}


const size_t OS::page_size = sysconf(_SC_PAGESIZE);
const size_t OS::page_mask = OS::page_size - 1;

u64 OS::nanotime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

u64 OS::millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

u64 OS::processStartTime() {
    static u64 start_time = 0;

    if (start_time == 0) {
        char buf[64];
        sprintf(buf, "/proc/%d", processId());

        struct stat st;
        if (stat(buf, &st) == 0) {
            start_time = (u64)st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000;
        }
    }

    return start_time;
}

void OS::sleep(u64 nanos) {
    struct timespec ts = {nanos / 1000000000, nanos % 1000000000};
    nanosleep(&ts, NULL);
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

int OS::processId() {
    static const int self_pid = getpid();

    return self_pid;
}

int OS::threadId() {
    return syscall(__NR_gettid);
}

const char* OS::schedPolicy() {
    int sched_policy = sched_getscheduler(0);
    if (sched_policy >= SCHED_BATCH) {
        return sched_policy >= SCHED_IDLE ? "[SCHED_IDLE]" : "[SCHED_BATCH]"; 
    }
    return "[SCHED_OTHER]";
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

SigAction OS::installSignalHandler(int signo, SigAction action, SigHandler handler) {
    struct sigaction sa;
    struct sigaction oldsa;
    sigemptyset(&sa.sa_mask);

    if (handler != NULL) {
        sa.sa_handler = handler;
        sa.sa_flags = 0;
    } else {
        sa.sa_sigaction = action;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
    }

    sigaction(signo, &sa, &oldsa);
    return oldsa.sa_sigaction;
}

bool OS::sendSignalToThread(int thread_id, int signo) {
    return syscall(__NR_tgkill, processId(), thread_id, signo) == 0;
}

void* OS::safeAlloc(size_t size) {
    // Naked syscall can be used inside a signal handler.
    // Also, we don't want to catch our own calls when profiling mmap.
    intptr_t result = syscall(MMAP_SYSCALL, NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result < 0 && result > -4096) {
        return NULL;
    }
    return (void*)result;
}

void OS::safeFree(void* addr, size_t size) {
    syscall(__NR_munmap, addr, size);
}

bool OS::getCpuDescription(char* buf, size_t size) {
    int fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd == -1) {
        return false;
    }

    ssize_t r = read(fd, buf, size);
    close(fd);
    if (r <= 0) {
        return false;
    }
    buf[r < size ? r : size - 1] = 0;

    char* c;
    do {
        c = strchr(buf, '\n');
    } while (c != NULL && *(buf = c + 1) != '\n');

    *buf = 0;
    return true;
}

u64 OS::getProcessCpuTime(u64* utime, u64* stime) {
    struct tms buf;
    clock_t real = times(&buf);
    *utime = buf.tms_utime;
    *stime = buf.tms_stime;
    return real;
}

u64 OS::getTotalCpuTime(u64* utime, u64* stime) {
    int fd = open("/proc/stat", O_RDONLY);
    if (fd == -1) {
        return (u64)-1;
    }

    u64 real = (u64)-1;
    char buf[512];
    if (read(fd, buf, sizeof(buf)) >= 12) {
        u64 user, nice, system, idle;
        if (sscanf(buf + 4, "%llu %llu %llu  %llu", &user, &nice, &system, &idle) == 4) {
            *utime = user + nice;
            *stime = system;
            real = user + nice + system + idle;
        }
    }

    close(fd);
    return real;
}

void OS::copyFile(int src_fd, int dst_fd, off_t offset, size_t size) {
    // copy_file_range() is probably better, but not supported on all kernels
    while (size > 0) {
        ssize_t bytes = sendfile(dst_fd, src_fd, &offset, size);
        if (bytes <= 0) {
            break;
        }
        size -= (size_t)bytes;
    }
}

#endif // __linux__
