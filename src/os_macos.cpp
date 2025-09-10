/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __APPLE__

#include <dlfcn.h>
#include <errno.h>
#include <libkern/OSByteOrder.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_time.h>
#include <mach/processor_info.h>
#include <mach/vm_map.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
#include "os.h"


class MacThreadList : public ThreadList {
  private:
    task_t _task;
    thread_array_t _thread_array;

    void deallocate() {
        if (_thread_array != NULL) {
            for (u32 i = 0; i < _count; i++) {
                mach_port_deallocate(_task, _thread_array[i]);
            }
            vm_deallocate(_task, (vm_address_t)_thread_array, _count * sizeof(thread_t));
            _thread_array = NULL;
        }
    }

  public:
    MacThreadList() {
        _task = mach_task_self();
        _thread_array = NULL;
        task_threads(_task, &_thread_array, &_count);
    }

    ~MacThreadList() {
        deallocate();
    }

    int next() {
        return (int)_thread_array[_index++];
    }

    void update() {
        deallocate();
        _index = _count = 0;
        task_threads(_task, &_thread_array, &_count);
    }
};


JitWriteProtection::JitWriteProtection(bool enable) {
#ifdef __aarch64__
    // Mimic pthread_jit_write_protect_np(), but save the previous state
    if (*(volatile char*)0xfffffc10c) {
        u64 val = enable ? *(volatile u64*)0xfffffc118 : *(volatile u64*)0xfffffc110;
        u64 prev;
        asm volatile("mrs %0, s3_6_c15_c1_5" : "=r" (prev) : : );
        if (prev != val) {
            _prev = prev;
            _restore = true;
            asm volatile("msr s3_6_c15_c1_5, %0\n"
                         "isb"
                         : "+r" (val) : : "memory");
            return;
        }
    }
    // Already in the required mode, or write protection is not supported
    _restore = false;
#endif
}

JitWriteProtection::~JitWriteProtection() {
#ifdef __aarch64__
    if (_restore) {
        u64 prev = _prev;
        asm volatile("msr s3_6_c15_c1_5, %0\n"
                     "isb"
                     : "+r" (prev) : : "memory");
    }
#endif
}


static SigAction installed_sigaction[32];
static SigAction orig_sigbus_handler;
static SigAction orig_sigsegv_handler;

const size_t OS::page_size = sysconf(_SC_PAGESIZE);
const size_t OS::page_mask = OS::page_size - 1;
const long OS::clock_ticks_per_sec = sysconf(_SC_CLK_TCK);

static mach_timebase_info_data_t timebase = {0, 0};

u64 OS::nanotime() {
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    return (u64)mach_absolute_time() * timebase.numer / timebase.denom;
}

u64 OS::micros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000000 + tv.tv_usec;
}

void OS::sleep(u64 nanos) {
    struct timespec ts = {(time_t)(nanos / 1000000000), (long)(nanos % 1000000000)};
    nanosleep(&ts, NULL);
}

void OS::uninterruptibleSleep(u64 nanos, volatile bool* flag) {
    struct timespec ts = {(time_t)(nanos / 1000000000), (long)(nanos % 1000000000)};
    while (*flag && nanosleep(&ts, &ts) < 0 && errno == EINTR);
}

u64 OS::overrun(siginfo_t* siginfo) {
    return 0;
}

u64 OS::processStartTime() {
    static u64 start_time = 0;

    if (start_time == 0) {
        struct proc_bsdinfo info;
        if (proc_pidinfo(processId(), PROC_PIDTBSDINFO, 0, &info, sizeof(info)) > 0) {
            start_time = (u64)info.pbi_start_tvsec * 1000 + info.pbi_start_tvusec / 1000;
        }
    }

    return start_time;
}

u64 OS::hton64(u64 x) {
    return OSSwapHostToBigInt64(x);
}

u64 OS::ntoh64(u64 x) {
    return OSSwapBigToHostInt64(x);
}

int OS::getMaxThreadId() {
    return 0x7fffffff;
}

int OS::processId() {
    static const int self_pid = getpid();

    return self_pid;
}

int OS::threadId() {
    // Used to be pthread_mach_thread_np(pthread_self()),
    // but pthread_mach_thread_np is not async signal safe
    mach_port_t port = mach_thread_self();
    mach_port_deallocate(mach_task_self(), port);
    return (int)port;
}

const char* OS::schedPolicy(int thread_id) {
    // Not used on macOS
    return "SCHED_OTHER";
}

bool OS::threadName(int thread_id, char* name_buf, size_t name_len) {
    pthread_t thread = pthread_from_mach_thread_np(thread_id);
    return thread && pthread_getname_np(thread, name_buf, name_len) == 0 && name_buf[0] != 0;
}

ThreadState OS::threadState(int thread_id) {
    struct thread_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    if (thread_info((thread_act_t)thread_id, THREAD_BASIC_INFO, (thread_info_t)&info, &size) != 0) {
        return THREAD_UNKNOWN;
    }
    return info.run_state == TH_STATE_RUNNING ? THREAD_RUNNING : THREAD_SLEEPING;
}

u64 OS::threadCpuTime(int thread_id) {
    if (thread_id == 0) thread_id = threadId();

    struct thread_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    if (thread_info((thread_act_t)thread_id, THREAD_BASIC_INFO, (thread_info_t)&info, &size) != 0) {
        return 0;
    }
    return u64(info.user_time.seconds + info.system_time.seconds) * 1000000000 +
           u64(info.user_time.microseconds + info.system_time.microseconds) * 1000;
}

ThreadList* OS::listThreads() {
    return new MacThreadList();
}

bool OS::isLinux() {
    return false;
}

bool OS::isMusl() {
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
        if (signo > 0 && signo < sizeof(installed_sigaction) / sizeof(installed_sigaction[0])) {
            installed_sigaction[signo] = action;
        }
    }

    sigaction(signo, &sa, &oldsa);
    return oldsa.sa_sigaction;
}

static void restoreSignalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    signal(signo, SIG_DFL);
}

SigAction OS::replaceCrashHandler(SigAction action) {
    // It is not well specified when macOS raises SIGBUS and when SIGSEGV.
    // HotSpot handles both similarly, so do we.
    struct sigaction sa;

    sigaction(SIGBUS, NULL, &sa);
    orig_sigbus_handler = sa.sa_handler == SIG_DFL ? restoreSignalHandler : sa.sa_sigaction;
    sa.sa_sigaction = action;
    sa.sa_flags |= SA_SIGINFO | SA_RESTART;
    sigaction(SIGBUS, &sa, NULL);

    sigaction(SIGSEGV, NULL, &sa);
    orig_sigsegv_handler = sa.sa_handler == SIG_DFL ? restoreSignalHandler : sa.sa_sigaction;
    sa.sa_sigaction = action;
    sa.sa_flags |= SA_SIGINFO | SA_RESTART;
    sigaction(SIGSEGV, &sa, NULL);

    // Return an action that dispatches to one of the original handlers depending on signo,
    // so that the caller does not need to deal with multiple handlers
    return [](int signo, siginfo_t* siginfo, void* ucontext) {
        (signo == SIGBUS ? orig_sigbus_handler : orig_sigsegv_handler)(signo, siginfo, ucontext);
    };
}

int OS::getProfilingSignal(int mode) {
    static int preferred_signals[2] = {SIGPROF, SIGVTALRM};

    const u64 allowed_signals =
        1ULL << SIGPROF | 1ULL << SIGVTALRM | 1ULL << SIGEMT | 1ULL << SIGSYS;

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
    } while ((signo = (signo + 1) & 31) != initial_signo);

    return signo;
}

bool OS::sendSignalToThread(int thread_id, int signo) {
#ifdef __aarch64__
    register long x0 asm("x0") = thread_id;
    register long x1 asm("x1") = signo;
    register long x16 asm("x16") = 328;
    asm volatile("svc #0x80"
                 : "+r" (x0)
                 : "r" (x1), "r" (x16)
                 : "memory");
    return x0 == 0;
#else
    int result;
    asm volatile("syscall"
                 : "=a" (result)
                 : "a" (0x2000148), "D" (thread_id), "S" (signo)
                 : "rcx", "r11", "memory");
    return result == 0;
#endif
}

void* OS::safeAlloc(size_t size) {
    // mmap() is not guaranteed to be async signal safe, but in practice, it is.
    // There is no a reasonable alternative anyway.
    void* result = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == MAP_FAILED) {
        return NULL;
    }
    return result;
}

void OS::safeFree(void* addr, size_t size) {
    munmap(addr, size);
}

bool OS::getCpuDescription(char* buf, size_t size) {
    return sysctlbyname("machdep.cpu.brand_string", buf, &size, NULL, 0) == 0;
}

int OS::getCpuCount() {
    int cpu_count;
    size_t size = sizeof(cpu_count);
    return sysctlbyname("hw.logicalcpu", &cpu_count, &size, NULL, 0) == 0 ? cpu_count : 1;
}

u64 OS::getProcessCpuTime(u64* utime, u64* stime) {
    struct tms buf;
    clock_t real = times(&buf);
    *utime = buf.tms_utime;
    *stime = buf.tms_stime;
    return real;
}

u64 OS::getTotalCpuTime(u64* utime, u64* stime) {
    natural_t cpu_count;
    processor_info_array_t cpu_info_array;
    mach_msg_type_number_t cpu_info_count;

    host_name_port_t host = mach_host_self();
    kern_return_t ret = host_processor_info(host, PROCESSOR_CPU_LOAD_INFO, &cpu_count, &cpu_info_array, &cpu_info_count);
    mach_port_deallocate(mach_task_self(), host);
    if (ret != 0) {
        return (u64)-1;
    }

    processor_cpu_load_info_data_t* cpu_load = (processor_cpu_load_info_data_t*)cpu_info_array;
    u64 user = 0;
    u64 system = 0;
    u64 idle = 0;
    for (natural_t i = 0; i < cpu_count; i++) {
        user += cpu_load[i].cpu_ticks[CPU_STATE_USER] + cpu_load[i].cpu_ticks[CPU_STATE_NICE];
        system += cpu_load[i].cpu_ticks[CPU_STATE_SYSTEM];
        idle += cpu_load[i].cpu_ticks[CPU_STATE_IDLE];
    }
    vm_deallocate(mach_task_self(), (vm_address_t)cpu_info_array, cpu_info_count * sizeof(int));

    *utime = user;
    *stime = system;
    return user + system + idle;
}

int OS::createMemoryFile(const char* name) {
    // Not supported on macOS
    return -1;
}

void OS::copyFile(int src_fd, int dst_fd, off_t offset, size_t size) {
    char* buf = (char*)mmap(NULL, size + offset, PROT_READ, MAP_PRIVATE, src_fd, 0);
    if (buf == NULL) {
        return;
    }

    while (size > 0) {
        ssize_t bytes = write(dst_fd, buf + offset, size < 262144 ? size : 262144);
        if (bytes <= 0) {
            break;
        }
        offset += (size_t)bytes;
        size -= (size_t)bytes;
    }

    munmap(buf, offset);
}

void OS::freePageCache(int fd, off_t start_offset) {
    // Not supported on macOS
}

int OS::mprotect(void* addr, size_t size, int prot) {
    if (prot & PROT_WRITE) prot |= VM_PROT_COPY;
    return vm_protect(mach_task_self(), (vm_address_t)addr, size, 0, prot);
}

// Checks if async-profiler is preloaded through the DYLD_INSERT_LIBRARIES mechanism.
// This is done by analyzing the order of loaded dynamic libraries.
bool OS::checkPreloaded() {
    if (getenv("DYLD_INSERT_LIBRARIES") == NULL) {
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

    uint32_t images = _dyld_image_count();
    for (uint32_t i = 0; i < images; i++) {
        void* image_base = (void*)_dyld_get_image_header(i);

        if (image_base == libprofiler.dli_fbase) {
            // async-profiler found first
            return true;
        } else if (image_base == libc.dli_fbase) {
            // libc found first
            return false;
        }
    }

    return false;
}

u64 OS::getSystemBootTime() {
    return 0;
}

u64 OS::getRamSize() {
    return 0;
}

int OS::getProcessIds(int* pids, int max_pids) {
    return 0;
}

bool OS::getBasicProcessInfo(int pid, ProcessInfo* info) {
    return false;
}

bool OS::getDetailedProcessInfo(ProcessInfo* info) {
    return false;
}

#endif // __APPLE__
