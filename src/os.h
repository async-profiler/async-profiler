/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OS_H
#define _OS_H

#include <signal.h>
#include <stddef.h>
#include <sys/types.h>
#include "arch.h"


typedef void (*SigAction)(int, siginfo_t*, void*);
typedef void (*SigHandler)(int);
typedef void (*TimerCallback)(void*);

// Interrupt threads with this signal. The same signal is used inside JDK to interrupt I/O operations.
const int WAKEUP_SIGNAL = SIGIO;

enum ThreadState {
    THREAD_UNKNOWN,
    THREAD_RUNNING,
    THREAD_SLEEPING
};

struct ProcessInfo {
    int pid = 0;
    int ppid = 0;
    char name[16];           // Process name from /proc/{pid}/stats
    char cmdline[2048];      // Command line from /proc/{pid}/cmdline
    unsigned int uid = 0;    // User ID
    unsigned char state = 0; // Process state (R, S, D, Z, T, etc.)
    u64 start_time = 0;      // Process start time (milliseconds since epoch)

    // CPU & thread stats
    float cpu_user = 0;    // User CPU time (seconds)
    float cpu_system = 0;  // System CPU time (seconds)
    float cpu_percent = 0; // CPU utilization percentage
    int threads = 0;       // Number of threads

    // Memory stats (in bytes)
    u64 vm_size = 0;   // Total virtual memory size
    u64 vm_rss = 0;    // Resident memory size
    u64 rss_anon = 0;  // Resident anonymous memory
    u64 rss_files = 0; // Resident file mappings
    u64 rss_shmem = 0; // Resident shared memory

    // Page fault stats
    u64 minor_faults = 0; // Minor page faults (no I/O required)
    u64 major_faults = 0; // Major page faults (I/O required)

    // I/O stats
    u64 io_read = 0;  // KB read from storage
    u64 io_write = 0; // KB written to storage
};


class ThreadList {
  protected:
    u32 _index;
    u32 _count;

    ThreadList() : _index(0), _count(0) {
    }

  public:
    virtual ~ThreadList() {}

    u32 index() const { return _index; }
    u32 count() const { return _count; }

    bool hasNext() const {
        return _index < _count;
    }

    virtual int next() = 0;
    virtual void update() = 0;
};


// W^X memory support
class JitWriteProtection {
  private:
    u64 _prev;
    bool _restore;

  public:
    JitWriteProtection(bool enable);
    ~JitWriteProtection();
};


class OS {
  public:
    static const size_t page_size;
    static const size_t page_mask;
    static const long clock_ticks_per_sec;

    static u64 nanotime();
    static u64 micros();
    static u64 processStartTime();
    static void sleep(u64 nanos);
    static void uninterruptibleSleep(u64 nanos, volatile bool* flag);
    static u64 overrun(siginfo_t* siginfo);

    static u64 hton64(u64 x);
    static u64 ntoh64(u64 x);

    static int getMaxThreadId();
    static int processId();
    static int threadId();
    static const char* schedPolicy(int thread_id);
    static bool threadName(int thread_id, char* name_buf, size_t name_len);
    static ThreadState threadState(int thread_id);
    static u64 threadCpuTime(int thread_id);
    static ThreadList* listThreads();

    static bool isLinux();
    static bool isMusl();

    static SigAction installSignalHandler(int signo, SigAction action, SigHandler handler = NULL);
    static SigAction replaceCrashHandler(SigAction action);
    static int getProfilingSignal(int mode);
    static bool sendSignalToThread(int thread_id, int signo);

    static void* safeAlloc(size_t size);
    static void safeFree(void* addr, size_t size);

    static bool getCpuDescription(char* buf, size_t size);
    static int getCpuCount();
    static u64 getProcessCpuTime(u64* utime, u64* stime);
    static u64 getTotalCpuTime(u64* utime, u64* stime);

    static int createMemoryFile(const char* name);
    static void copyFile(int src_fd, int dst_fd, off_t offset, size_t size);
    static void freePageCache(int fd, off_t start_offset);
    static int mprotect(void* addr, size_t size, int prot);

    static bool checkPreloaded();

    static u64 getSystemBootTime();
    static u64 getRamSize();
    static int getProcessIds(int* pids, int max_pids);
    static bool getBasicProcessInfo(int pid, ProcessInfo* info);
    static bool getDetailedProcessInfo(ProcessInfo* info);
};

#endif // _OS_H
