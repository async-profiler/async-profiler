/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>
#include "j9StackTraces.h"
#include "j9Ext.h"
#include "profiler.h"
#include "perfEvents.h"
#include "tsc.h"


enum {
    J9_STOPPED = 0x40,
    J9_HALT_THREAD_INSPECTION = 0x8000
};

class J9VMThread {
  private:
    uintptr_t _unused1[10];
    uintptr_t _overflow_mark;
    uintptr_t _unused2[8];
    uintptr_t _flags;

  public:
    uintptr_t getAndSetFlag(uintptr_t flag) {
        return __sync_fetch_and_or(&_flags, flag);
    }

    void clearFlag(uintptr_t flag) {
        __sync_fetch_and_and(&_flags, ~flag);
    }

    void setOverflowMark() {
        __atomic_store_n(&_overflow_mark, (uintptr_t)-1, __ATOMIC_RELEASE);
    }
};


pthread_t J9StackTraces::_thread = 0;
int J9StackTraces::_max_stack_depth;
int J9StackTraces::_pipe[2];

static JNIEnv* _self_env = NULL;


Error J9StackTraces::start(Arguments& args) {
    _max_stack_depth = args._jstackdepth;

    if (pipe(_pipe) != 0) {
        return Error("Failed to create pipe");
    }
    fcntl(_pipe[1], F_SETFL, O_NONBLOCK);

    if (pthread_create(&_thread, NULL, threadEntry, NULL) != 0) {
        close(_pipe[0]);
        close(_pipe[1]);
        return Error("Unable to create sampler thread");
    }

    return Error::OK;
}

void J9StackTraces::stop() {
    if (_thread != 0) {
        close(_pipe[1]);
        pthread_join(_thread, NULL);
        close(_pipe[0]);
        _thread = 0;
    }
}

void J9StackTraces::timerLoop() {
    JNIEnv* jni = VM::attachThread("Async-profiler Sampler");
    __atomic_store_n(&_self_env, jni, __ATOMIC_RELEASE);

    jni->PushLocalFrame(64);

    jvmtiEnv* jvmti = VM::jvmti();
    char notification_buf[65536];
    std::map<void*, jthread> known_threads;

    int max_frames = _max_stack_depth + MAX_J9_NATIVE_FRAMES + RESERVED_FRAMES;
    ASGCT_CallFrame* frames = (ASGCT_CallFrame*)malloc(max_frames * sizeof(ASGCT_CallFrame));
    jvmtiFrameInfoExtended* jvmti_frames = (jvmtiFrameInfoExtended*)malloc(max_frames * sizeof(jvmtiFrameInfoExtended));

    while (true) {
        ssize_t bytes = read(_pipe[0], notification_buf, sizeof(notification_buf));
        if (bytes <= 0) {
            if (bytes < 0 && errno == EAGAIN) {
                continue;
            }
            break;
        }

        ssize_t ptr = 0;
        while (ptr < bytes) {
            J9StackTraceNotification* notif = (J9StackTraceNotification*)(notification_buf + ptr);
            u64 start_time = TSC::ticks();

            jthread thread = known_threads[notif->env];
            jint num_jvmti_frames;
            if (thread == NULL || J9Ext::GetStackTraceExtended(thread, 0, _max_stack_depth, jvmti_frames, &num_jvmti_frames) != 0) {
                jni->PopLocalFrame(NULL);
                jni->PushLocalFrame(64);

                jint thread_count;
                jthread* threads;
                if (jvmti->GetAllThreads(&thread_count, &threads) == 0) {
                    known_threads.clear();
                    for (jint i = 0; i < thread_count; i++) {
                        known_threads[J9Ext::GetJ9vmThread(threads[i])] = threads[i];
                    }
                    jvmti->Deallocate((unsigned char*)threads);
                }

                if ((thread = known_threads[notif->env]) == NULL ||
                    J9Ext::GetStackTraceExtended(thread, 0, _max_stack_depth, jvmti_frames, &num_jvmti_frames) != 0) {
                    continue;
                }
            }

            int num_frames = Profiler::instance()->convertNativeTrace(notif->num_frames, notif->addr, frames, EXECUTION_SAMPLE);

            for (int j = 0; j < num_jvmti_frames; j++) {
                frames[num_frames].method_id = jvmti_frames[j].method;
                frames[num_frames].bci = FrameType::encode(jvmti_frames[j].type, jvmti_frames[j].location);
                num_frames++;
            }

            int tid = J9Ext::GetOSThreadID(thread);
            ExecutionEvent event(start_time);
            Profiler::instance()->recordExternalSample(notif->counter, tid, EXECUTION_SAMPLE, &event, num_frames, frames);

            ptr += notif->size();
        }
    }

    free(jvmti_frames);
    free(frames);

    __atomic_store_n(&_self_env, NULL, __ATOMIC_RELEASE);
    VM::detachThread();
}

void J9StackTraces::checkpoint(u64 counter, J9StackTraceNotification* notif) {
    JNIEnv* self_env = __atomic_load_n(&_self_env, __ATOMIC_ACQUIRE);
    if (self_env == NULL) {
        // Sampler thread is not ready
        return;
    }

    JNIEnv* env = VM::jni();
    if (env != NULL && env != self_env) {
        J9VMThread* vm_thread = (J9VMThread*)env;
        uintptr_t flags = vm_thread->getAndSetFlag(J9_HALT_THREAD_INSPECTION);
        if (flags & J9_HALT_THREAD_INSPECTION) {
            // Thread is already scheduled for inspection, no need to notify again
            return;
        } else if (!(flags & J9_STOPPED)) {
            vm_thread->setOverflowMark();
            notif->env = env;
            notif->counter = counter;
            if (write(_pipe[1], notif, notif->size()) > 0) {
                return;
            }
        }
        // Something went wrong - rollback
        vm_thread->clearFlag(J9_HALT_THREAD_INSPECTION);
    }
}
