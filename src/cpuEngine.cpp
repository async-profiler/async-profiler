/*
 * Copyright 2023 Andrei Pangin
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

#include <pthread.h>
#include "cpuEngine.h"
#include "j9StackTraces.h"
#include "os.h"
#include "profiler.h"
#include "stackWalker.h"
#include "vmStructs.h"


void** CpuEngine::_pthread_entry = NULL;
CpuEngine* CpuEngine::_current = NULL;

long CpuEngine::_interval;
CStack CpuEngine::_cstack;
int CpuEngine::_signal;

// Intercept thread creation/termination by patching libjvm's GOT entry for pthread_setspecific().
// HotSpot puts VMThread into TLS on thread start, and resets on thread end.
static int pthread_setspecific_hook(pthread_key_t key, const void* value) {
    if (key != VMThread::key()) {
        return pthread_setspecific(key, value);
    }
    if (pthread_getspecific(key) == value) {
        return 0;
    }

    if (value != NULL) {
        int result = pthread_setspecific(key, value);
        CpuEngine::onThreadStart();
        return result;
    } else {
        CpuEngine::onThreadEnd();
        return pthread_setspecific(key, value);
    }
}

void CpuEngine::onThreadStart() {
    CpuEngine* current = __atomic_load_n(&_current, __ATOMIC_ACQUIRE);
    if (current != NULL) {
        current->createForThread(OS::threadId());
    }
}

void CpuEngine::onThreadEnd() {
    CpuEngine* current = __atomic_load_n(&_current, __ATOMIC_ACQUIRE);
    if (current != NULL) {
        current->destroyForThread(OS::threadId());
    }
}

bool CpuEngine::setupThreadHook() {
    if (_pthread_entry != NULL) {
        return true;
    }

    if (!VM::loaded()) {
        static void* dummy_pthread_entry;
        _pthread_entry = &dummy_pthread_entry;
        return true;
    }

    // Depending on Zing version, pthread_setspecific is called either from libazsys.so or from libjvm.so
    if (VM::isZing()) {
        CodeCache* libazsys = Profiler::instance()->findLibraryByName("libazsys");
        if (libazsys != NULL && (_pthread_entry = libazsys->findImport(im_pthread_setspecific)) != NULL) {
            return true;
        }
    }

    CodeCache* lib = Profiler::instance()->findJvmLibrary("libj9thr");
    return lib != NULL && (_pthread_entry = lib->findImport(im_pthread_setspecific)) != NULL;
}

void CpuEngine::enableThreadHook() {
    *_pthread_entry = (void*)pthread_setspecific_hook;
    __atomic_store_n(&_current, this, __ATOMIC_RELEASE);
}

void CpuEngine::disableThreadHook() {
    *_pthread_entry = (void*)pthread_setspecific;
    __atomic_store_n(&_current, NULL, __ATOMIC_RELEASE);
}

int CpuEngine::createForAllThreads() {
    int result = 1;

    ThreadList* thread_list = OS::listThreads();
    for (int tid; (tid = thread_list->next()) != -1; ) {
        int err = createForThread(tid);
        if (result != 0) result = err;
    }
    delete thread_list;

    return result;
}

void CpuEngine::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (!_enabled) return;

    ExecutionEvent event;
    Profiler::instance()->recordSample(ucontext, _interval, EXECUTION_SAMPLE, &event);
}

void CpuEngine::signalHandlerJ9(int signo, siginfo_t* siginfo, void* ucontext) {
    if (!_enabled) return;

    J9StackTraceNotification notif;
    StackContext java_ctx;
    notif.num_frames = _cstack == CSTACK_NO ? 0 : _cstack == CSTACK_DWARF
        ? StackWalker::walkDwarf(ucontext, notif.addr, MAX_J9_NATIVE_FRAMES, &java_ctx)
        : StackWalker::walkFP(ucontext, notif.addr, MAX_J9_NATIVE_FRAMES, &java_ctx);
    J9StackTraces::checkpoint(_interval, &notif);
}
