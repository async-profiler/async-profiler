/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include <arpa/inet.h>
#include <byteswap.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
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
    int* _thread_array;
    u32 _capacity;

    void addThread(int thread_id) {
        if (_count >= _capacity) {
            _capacity = _count * 2;
            _thread_array = (int*)realloc(_thread_array, _capacity * sizeof(int));
        }
        _thread_array[_count++] = thread_id;
    }

    void fillThreadArray() {
        if (_dir != NULL) {
            rewinddir(_dir);
            struct dirent* entry;
            while ((entry = readdir(_dir)) != NULL) {
                if (entry->d_name[0] != '.') {
                    addThread(atoi(entry->d_name));
                }
            }
        }
    }

  public:
    LinuxThreadList() : ThreadList() {
        _dir = opendir("/proc/self/task");
        _capacity = 128;
        _thread_array = (int*)malloc(_capacity * sizeof(int));
        fillThreadArray();
    }

    ~LinuxThreadList() {
        free(_thread_array);
        if (_dir != NULL) {
            closedir(_dir);
        }
    }

    int next() {
        return _thread_array[_index++];
    }

    void update() {
        _index = _count = 0;
        fillThreadArray();
    }
};


JitWriteProtection::JitWriteProtection(bool enable) {
    // Not used on Linux
}

JitWriteProtection::~JitWriteProtection() {
    // Not used on Linux
}


static SigAction installed_sigaction[64];

const size_t OS::page_size = sysconf(_SC_PAGESIZE);
const size_t OS::page_mask = OS::page_size - 1;
const long OS::clock_ticks_per_sec = sysconf(_SC_CLK_TCK);


u64 OS::nanotime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

u64 OS::micros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000000 + tv.tv_usec;
}

u64 OS::processStartTime() {
    static u64 start_time = 0;

    if (start_time == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "/proc/%d", processId());

        struct stat st;
        if (stat(buf, &st) == 0) {
            start_time = (u64)st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000;
        }
    }

    return start_time;
}

void OS::sleep(u64 nanos) {
    struct timespec ts = {(time_t)(nanos / 1000000000), (long)(nanos % 1000000000)};
    nanosleep(&ts, NULL);
}

u64 OS::overrun(siginfo_t* siginfo) {
    return siginfo->si_overrun;
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

const char* OS::schedPolicy(int thread_id) {
    int sched_policy = sched_getscheduler(thread_id);
    if (sched_policy >= SCHED_BATCH) {
        return sched_policy >= SCHED_IDLE ? "SCHED_IDLE" : "SCHED_BATCH";
    }
    return "SCHED_OTHER";
}

bool OS::threadName(int thread_id, char* name_buf, size_t name_len) {
    char buf[64];
    snprintf(buf, sizeof(buf), "/proc/self/task/%d/comm", thread_id);
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
    snprintf(buf, sizeof(buf), "/proc/self/task/%d/stat", thread_id);
    int fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return THREAD_UNKNOWN;
    }

    ThreadState state = THREAD_UNKNOWN;
    if (read(fd, buf, sizeof(buf)) > 0) {
        char* s = strchr(buf, ')');
        state = s != NULL && (s[2] == 'R' || s[2] == 'D') ? THREAD_RUNNING : THREAD_SLEEPING;
    }

    close(fd);
    return state;
}

u64 OS::threadCpuTime(int thread_id) {
    clockid_t thread_cpu_clock;
    if (thread_id) {
        thread_cpu_clock = ((~(unsigned int)(thread_id)) << 3) | 6;  // CPUCLOCK_SCHED | CPUCLOCK_PERTHREAD_MASK
    } else {
        thread_cpu_clock = CLOCK_THREAD_CPUTIME_ID;
    }

    struct timespec ts;
    if (clock_gettime(thread_cpu_clock, &ts) == 0) {
        return (u64)ts.tv_sec * 1000000000 + ts.tv_nsec;
    }
    return 0;
}

ThreadList* OS::listThreads() {
    return new LinuxThreadList();
}

bool OS::isLinux() {
    return true;
}

// _CS_GNU_LIBC_VERSION is not defined on musl
const static bool musl = confstr(_CS_GNU_LIBC_VERSION, NULL, 0) == 0 && errno != 0;

bool OS::isMusl() {
    return musl;
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
        if (signo > 0 && signo < sizeof(installed_sigaction) / sizeof(installed_sigaction[0])) {
            installed_sigaction[signo] = action;
        }
    }

    sigaction(signo, &sa, &oldsa);
    return oldsa.sa_sigaction;
}

SigAction OS::replaceCrashHandler(SigAction action) {
    struct sigaction sa;
    sigaction(SIGSEGV, NULL, &sa);
    SigAction old_action = sa.sa_sigaction;
    sa.sa_sigaction = action;
    sa.sa_flags |= SA_SIGINFO | SA_RESTART;
    sigaction(SIGSEGV, &sa, NULL);
    return old_action;
}

int OS::getProfilingSignal(int mode) {
    static int preferred_signals[2] = {SIGPROF, SIGVTALRM};

    const u64 allowed_signals =
        1ULL << SIGPROF | 1ULL << SIGVTALRM | 1ULL << SIGSTKFLT | 1ULL << SIGPWR | -(1ULL << SIGRTMIN);

    int& signo = preferred_signals[mode];
    int initial_signo = signo;
    int other_signo = preferred_signals[1 - mode];

    do {
        struct sigaction sa;
        if ((allowed_signals & (1ULL << signo)) != 0 && signo != other_signo && sigaction(signo, NULL, &sa) == 0) {
            if (sa.sa_handler == SIG_DFL || sa.sa_handler == SIG_IGN || sa.sa_sigaction == installed_sigaction[signo]) {
                return signo;
            }
        }
    } while ((signo = (signo + 53) & 63) != initial_signo);

    return signo;
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

int OS::getCpuCount() {
    return sysconf(_SC_NPROCESSORS_ONLN);
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
    char buf[128] = {0};
    if (read(fd, buf, sizeof(buf)) >= 12) {
        u64 user, nice, system, idle;
        if (sscanf(buf + 4, "%llu %llu %llu %llu", &user, &nice, &system, &idle) == 4) {
            *utime = user + nice;
            *stime = system;
            real = user + nice + system + idle;
        }
    }

    close(fd);
    return real;
}

int OS::createMemoryFile(const char* name) {
    return syscall(__NR_memfd_create, name, 0);
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

void OS::freePageCache(int fd, off_t start_offset) {
    posix_fadvise(fd, start_offset & ~page_mask, 0, POSIX_FADV_DONTNEED);
}

int OS::mprotect(void* addr, size_t size, int prot) {
    return ::mprotect(addr, size, prot);
}

static int checkPreloadedCallback(dl_phdr_info* info, size_t size, void* data) {
    Dl_info* dl_info = (Dl_info*)data;

    Dl_info libprofiler = dl_info[0];
    Dl_info libc = dl_info[1];

    if ((void*)info->dlpi_addr == libprofiler.dli_fbase) {
        // async-profiler found first
        return 1;
    } else if ((void*)info->dlpi_addr == libc.dli_fbase) {
        // libc found first
        return -1;
    }

    return 0;
}

// Checks if async-profiler is preloaded through the LD_PRELOAD mechanism.
// This is done by analyzing the order of loaded dynamic libraries.
bool OS::checkPreloaded() {
    if (getenv("LD_PRELOAD") == NULL) {
        return false;
    }

    // Find async-profiler shared object
    Dl_info libprofiler;
    if (dladdr((const void*)OS::checkPreloaded, &libprofiler) == 0) {
        return false;
    }

    // Find libc shared object
    Dl_info libc;
    if (dladdr((const void*)exit, &libc) == 0) {
        return false;
    }

    Dl_info info[2] = {libprofiler, libc};
    return dl_iterate_phdr(checkPreloadedCallback, (void*)info) == 1;
}

// Helper function to check if a string is a number (PID)
static bool isNumber(const char* str) {
    if (!str || *str == '\0') return false;
    for (const char* p = str; *p; p++) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

void OS::getProcessIds(int* pids, int* count, int max_pids) {
    *count = 0;

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return;

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) && *count < max_pids) {
        if (entry->d_type == DT_DIR && isNumber(entry->d_name)) {
            pids[*count] = atoi(entry->d_name);
            (*count)++;
        }
    }

    closedir(proc_dir);
    fprintf(stderr, "found the process count to be %d\n", *count);
}

// Helper method to read process name from /proc/{pid}/comm
static bool readProcessName(int pid, ProcessInfo* info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE* file = fopen(path, "r");
    if (file) {
        if (fgets(info->_name, sizeof(info->_name), file)) {
            // Remove trailing newline
            size_t len = strlen(info->_name);
            if (len > 0 && info->_name[len-1] == '\n') {
                info->_name[len-1] = '\0';
            }
        }
        fclose(file);
    }
    if (info->_name[0] == '\0') {
        snprintf(info->_name, sizeof(info->_name), "pid-%d", pid);
    }
    return true;
}

// Helper method to read command line from /proc/{pid}/cmdline
static bool readProcessCmdline(int pid, ProcessInfo* info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE* file = fopen(path, "rb");  // Use binary mode to handle null bytes
    if (file) {
        size_t len = fread(info->_cmdline, 1, sizeof(info->_cmdline) - 1, file);
        if (len > 0) {
            // Replace null bytes with spaces (arguments are separated by null bytes)
            for (size_t i = 0; i < len; i++) {
                if (info->_cmdline[i] == '\0') {
                    info->_cmdline[i] = ' ';
                }
            }
            // Ensure null termination
            info->_cmdline[len] = '\0';
            // Remove trailing space if present
            if (len > 0 && info->_cmdline[len-1] == ' ') {
                info->_cmdline[len-1] = '\0';
            }
        }
        fclose(file);
    }
    return true;
}

// Helper method to read process stats from /proc/{pid}/stat
static bool readProcessStats(int pid, ProcessInfo* info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE* file = fopen(path, "r");
    if (!file) return false;  // Critical failure

    char buffer[1024];
    if (!fgets(buffer, sizeof(buffer), file)) {
        fclose(file);
        return false;
    }

    // Parse the stat file - we need specific fields
    // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime cutime cstime priority nice num_threads itrealvalue starttime vsize rss ...
    int parsed_pid, parsed_ppid;
    char state;
    unsigned long minflt, majflt, utime, stime, starttime;
    int threads;

    int parsed = sscanf(buffer, "%d %*s %c %d %*d %*d %*d %*d %*u %lu %*u %lu %*u %lu %lu %*d %*d %*d %*d %d %*d %lu",
                       &parsed_pid, &state, &parsed_ppid, &minflt, &majflt, &utime, &stime, &threads, &starttime);

    fclose(file);

    if (parsed >= 9) {
        info->_ppid = parsed_ppid;
        info->_state = (unsigned char)state;
        info->_minor_faults = minflt;
        info->_major_faults = majflt;
        info->_cpu_user = utime;
        info->_cpu_system = stime;
        info->_threads = (unsigned short)threads;
        info->_start_time = starttime;
        return true;
    } else {
        return false;  // Critical parsing failure
    }
}

// Helper method to read memory stats from /proc/{pid}/statm
static bool readProcessMemory(int pid, ProcessInfo* info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/statm", pid);
    FILE* file = fopen(path, "r");
    if (file) {
        unsigned long size, resident, shared, text, lib, data_stack, dt;
        int parsed = fscanf(file, "%lu %lu %lu %lu %lu %lu %lu",
                           &size, &resident, &shared, &text, &lib, &data_stack, &dt);
        fclose(file);

        if (parsed >= 6) {
            info->_mem_size = size;
            info->_mem_resident = resident;
            info->_mem_shared = shared;
            info->_mem_text = text;
            info->_mem_data = data_stack;  // data + stack
            return true;
        }
    }
    return false;
}

// Helper method to read UID from /proc/{pid}/status
static bool readProcessUID(int pid, ProcessInfo* info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* file = fopen(path, "r");
    if (file) {
        char line[256];
        int lines_read = 0;
        while (fgets(line, sizeof(line), file) && lines_read < 10) {  // UID is usually in first few lines
            lines_read++;
            if (strncmp(line, "Uid:", 4) == 0) {
                unsigned int uid;
                if (sscanf(line + 4, "%u", &uid) == 1) {
                    info->_uid = uid;
                    fclose(file);
                    return true;
                }
            }
        }
        fclose(file);
    }
    return false;
}

// Helper method to read I/O stats from /proc/{pid}/io
static bool readProcessIO(int pid, ProcessInfo* info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    FILE* file = fopen(path, "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "read_bytes:", 11) == 0) {
                unsigned long read_bytes;
                if (sscanf(line + 11, "%lu", &read_bytes) == 1) {
                    info->_io_read = read_bytes;
                }
            } else if (strncmp(line, "write_bytes:", 12) == 0) {
                unsigned long write_bytes;
                if (sscanf(line + 12, "%lu", &write_bytes) == 1) {
                    info->_io_write = write_bytes;
                }
            }
        }
        fclose(file);
        return true;
    }
    return false;
}

bool OS::getProcessInfo(int pid, ProcessInfo* info) {
    *info = ProcessInfo();
    info->_pid = pid;

    if (!readProcessStats(pid, info)) {
        return false;
    }

    readProcessName(pid, info);
    readProcessCmdline(pid, info);

    readProcessMemory(pid, info);
    readProcessUID(pid, info);
    readProcessIO(pid, info);

    return true;
}

#endif // __linux__
