/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <pthread.h>
#include "cpuEngine.h"
#include "j9StackTraces.h"
#include "profiler.h"
#include "stackWalker.h"
#include "tsc.h"
#include "vmStructs.h"


void** CpuEngine::_pthread_entry = NULL;
CpuEngine* CpuEngine::_current = NULL;

long CpuEngine::_interval;
CStack CpuEngine::_cstack;
int CpuEngine::_signal;
bool CpuEngine::_count_overrun;

void CpuEngine::onThreadStart() {
    CpuEngine* current = loadAcquire(_current);
    if (current != NULL) {
        current->createForThread(OS::threadId());
    }
}

void CpuEngine::onThreadEnd() {
    CpuEngine* current = loadAcquire(_current);
    if (current != NULL) {
        current->destroyForThread(OS::threadId());
    }
}


void CpuEngine::enableEngine() {
    storeRelease(_current, this);
}

void CpuEngine::disableEngine() {
    storeRelease(_current, nullptr);
}

bool CpuEngine::isResourceLimit(int err) {
    return err == EMFILE || err == ENOMEM;
}

int CpuEngine::createForAllThreads() {
    int result = EPERM;

    ThreadList* thread_list = OS::listThreads();
    while (thread_list->hasNext()) {
        int tid = thread_list->next();
        int err = createForThread(tid);
        if (isResourceLimit(err)) {
            result = err;
            break;
        } else if (result != 0) {
            result = err;
        }
    }
    delete thread_list;

    return result;
}

void CpuEngine::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (!_enabled) return;

    ExecutionEvent event(TSC::ticks());
    // Count missed samples when estimating total CPU time
    u64 total_cpu_time = _count_overrun ? u64(_interval) * (1 + OS::overrun(siginfo)) : u64(_interval);
    Profiler::instance()->recordSample(ucontext, total_cpu_time, EXECUTION_SAMPLE, &event);
}

void CpuEngine::signalHandlerJ9(int signo, siginfo_t* siginfo, void* ucontext) {
    if (!_enabled) return;

    J9StackTraceNotification notif;
    notif.num_frames = _cstack == CSTACK_NO ? 0 : _cstack == CSTACK_DWARF
        ? StackWalker::walkDwarf(ucontext, notif.addr, MAX_J9_NATIVE_FRAMES)
        : StackWalker::walkFP(ucontext, notif.addr, MAX_J9_NATIVE_FRAMES);
    J9StackTraces::checkpoint(_interval, &notif);
}
