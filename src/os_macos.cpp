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

#include <libkern/OSByteOrder.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/time.h>
#include "os.h"


class MacThreadList : public ThreadList {
  private:
    thread_array_t _thread_array;
    unsigned int _thread_count;
    unsigned int _thread_index;

  public:
    MacThreadList() : _thread_array(NULL), _thread_count(0), _thread_index(0) {
        task_threads(mach_task_self(), &_thread_array, &_thread_count);
    }

    ~MacThreadList() {
        task_t task = mach_task_self();
        for (int i = 0; i < _thread_count; i++) {
            mach_port_deallocate(task, _thread_array[i]);
        }
        vm_deallocate(task, (vm_address_t)_thread_array, sizeof(thread_t) * _thread_count);
    }

    int next() {
        if (_thread_index < _thread_count) {
            return (int)_thread_array[_thread_index++];
        }
        return -1;
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

u64 OS::hton64(u64 x) {
    return OSSwapHostToBigInt64(x);
}

int OS::threadId() {
    // Used to be pthread_mach_thread_np(pthread_self()),
    // but pthread_mach_thread_np is not async signal safe
    mach_port_t port = mach_thread_self();
    mach_port_deallocate(mach_task_self(), port);
    return (int)port;
}

bool OS::isThreadRunning(int thread_id) {
    struct thread_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    if (thread_info((thread_act_t)thread_id, THREAD_BASIC_INFO, (thread_info_t)&info, &size) != 0) {
        return false;
    }
    return info.run_state == TH_STATE_RUNNING;
}

bool OS::isSignalSafeTLS() {
    return false;
}

bool OS::isJavaLibraryVisible() {
    return true;
}

void OS::installSignalHandler(int signo, void (*handler)(int, siginfo_t*, void*)) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;

    sigaction(signo, &sa, NULL);
}

void OS::sendSignalToThread(int thread_id, int signo) {
   asm volatile("syscall" : : "a"(0x2000148), "D"(thread_id), "S"(signo));
}

ThreadList* OS::listThreads() {
    return new MacThreadList();
}

#endif // __APPLE__
