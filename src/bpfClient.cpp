/*
 * Copyright 2021 Andrei Pangin
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

#include <unistd.h>
#include <sys/mman.h>
#include "bpfClient.h"
#include "fdtransferClient.h"
#include "os.h"
#include "profiler.h"
#include "vmStructs.h"


// Use different profiling signal to allow running two profilers together
const int BPF_SIGNAL = SIGSTKFLT;

struct BpfStackTrace {
    u32 pid;
    u32 tid;
    u64 counter;
    u16 event_type;
    u16 sched_policy;
    u32 depth;
    u64 ip[0];
};

struct BpfMap {
    char* addr;
    size_t size;
    u32 salt;
    u32 mask;
    u32 entry_size;

    BpfStackTrace* getStackForThread(u32 tid) const {
        size_t index = (salt + tid) & mask;
        return (BpfStackTrace*)(addr + index * entry_size);
    }
};

static BpfMap _bpf_map;
static int _bpf_map_available = 0;

static unsigned int _interval;
static unsigned int _counter;


void BpfClient::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (!_enabled) return;

    if (_interval <= 1 || __sync_add_and_fetch(&_counter, 1) % _interval == 0) {
        ExecutionEvent event;
        Profiler::instance()->recordSample(ucontext, 1, 0, &event);
    }
}

Error BpfClient::check(Arguments& args) {
    return Error::OK;
}

Error BpfClient::start(Arguments& args) {
    OS::installSignalHandler(BPF_SIGNAL, signalHandler);

    struct bpfmap_params params;
    int fd = FdTransferClient::requestBpfMapFd(&params);
    if (fd < 0) {
        return Error("Failed to request bpf map");
    }

    _interval = args._interval;
    _counter = 0;

    _bpf_map.salt = params.salt;
    _bpf_map.mask = params.num_entries - 1;
    _bpf_map.entry_size = params.entry_size;
    _bpf_map.size = (size_t)params.entry_size * params.num_entries;

    _bpf_map.addr = (char*)mmap(NULL, _bpf_map.size, PROT_READ, MAP_SHARED, fd, 0);
    if (_bpf_map.addr == MAP_FAILED) {
        close(fd);
        return Error("Failed to mmap stack trace buffer");
    }

    close(fd);
    __atomic_store_n(&_bpf_map_available, 1, __ATOMIC_RELEASE);

    return Error::OK;
}

void BpfClient::stop() {
    OS::installSignalHandler(BPF_SIGNAL, NULL, SIG_IGN);

    __atomic_store_n(&_bpf_map_available, 0, __ATOMIC_RELEASE);
    munmap(_bpf_map.addr, _bpf_map.size);
}

int BpfClient::getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth) {
    if (!__atomic_load_n(&_bpf_map_available, __ATOMIC_ACQUIRE)) {
        return 0;
    }

    BpfStackTrace* trace = _bpf_map.getStackForThread(tid);
    if (trace->tid != tid) {
        return 0;
    }

    int depth = 0;
    if (trace->depth < max_depth) max_depth = trace->depth;

    while (depth < max_depth) {
        const void* ip = (const void*)trace->ip[depth];
        if (CodeHeap::contains(ip)) {
            // Stop at the first Java frame
            break;
        }
        callchain[depth++] = ip;
    }

    return depth;
}
