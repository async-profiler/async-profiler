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

#ifdef __APPLE__

#include <dispatch/dispatch.h>
#include <libkern/OSByteOrder.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_time.h>
#include <mach/processor_info.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/times.h>
#include "os.h"


class MacThreadList : public ThreadList {
  private:
    task_t _task;
    thread_array_t _thread_array;
    unsigned int _thread_count;
    unsigned int _thread_index;

    void ensureThreadArray() {
        if (_thread_array == NULL) {
            _thread_count = 0;
            _thread_index = 0;
            task_threads(_task, &_thread_array, &_thread_count);
        }
    }

  public:
    MacThreadList() {
        _task = mach_task_self();
        _thread_array = NULL;
    }

    ~MacThreadList() {
        rewind();
    }

    void rewind() {
        if (_thread_array != NULL) {
            for (int i = 0; i < _thread_count; i++) {
                mach_port_deallocate(_task, _thread_array[i]);
            }
            vm_deallocate(_task, (vm_address_t)_thread_array, _thread_count * sizeof(thread_t));
            _thread_array = NULL;
        }
    }

    int next() {
        ensureThreadArray();
        if (_thread_index < _thread_count) {
            return (int)_thread_array[_thread_index++];
        }
        return -1;
    }

    int size() {
        ensureThreadArray();
        return _thread_count;
    }
};


static mach_timebase_info_data_t timebase = {0, 0};

u64 OS::nanotime() {
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    return (u64)mach_absolute_time() * timebase.numer / timebase.denom;
}

u64 OS::millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000 + tv.tv_usec / 1000;
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

bool OS::threadName(int thread_id, char* name_buf, size_t name_len) {
    pthread_t thread = pthread_from_mach_thread_np(thread_id);
    return thread && pthread_getname_np(thread, name_buf, name_len) == 0 && name_buf[0] != 0;
}

ThreadState OS::threadState(int thread_id) {
    struct thread_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    if (thread_info((thread_act_t)thread_id, THREAD_BASIC_INFO, (thread_info_t)&info, &size) != 0) {
        return THREAD_INVALID;
    }
    return info.run_state == TH_STATE_RUNNING ? THREAD_RUNNING : THREAD_SLEEPING;
}

ThreadList* OS::listThreads() {
    return new MacThreadList();
}

bool OS::isJavaLibraryVisible() {
    return true;
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
   int result;
   asm volatile("syscall"
                : "=a" (result)
                : "a" (0x2000148), "D" (thread_id), "S" (signo)
                : "rcx", "r11", "memory");
   return result == 0;
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

Timer* OS::startTimer(u64 interval, TimerCallback callback, void* arg) {
    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    if (source != NULL) {
        dispatch_source_set_timer(source, dispatch_time(DISPATCH_TIME_NOW, interval), interval, 0);
        dispatch_source_set_event_handler_f(source, callback);
        dispatch_set_context(source, arg);
        dispatch_resume(source);
    }
    return (Timer*)source;
}

void OS::stopTimer(Timer* timer) {
    dispatch_source_t source = (dispatch_source_t)timer;
    if (source != NULL) {
        dispatch_source_cancel(source);
        dispatch_release(source);
    }
}

bool OS::getCpuDescription(char* buf, size_t size) {
    return sysctlbyname("machdep.cpu.brand_string", buf, &size, NULL, 0) == 0;
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

#endif // __APPLE__
