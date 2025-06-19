/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "j9WallClock.h"
#include "j9Ext.h"
#include "profiler.h"
#include "tsc.h"


long J9WallClock::_interval;

Error J9WallClock::start(Arguments& args) {
    _interval = args._interval ? args._interval : DEFAULT_INTERVAL * 5;
    _max_stack_depth = args._jstackdepth;

    _running = true;

    if (pthread_create(&_thread, NULL, threadEntry, this) != 0) {
        return Error("Unable to create timer thread");
    }

    return Error::OK;
}

void J9WallClock::stop() {
    _running = false;
    pthread_kill(_thread, WAKEUP_SIGNAL);
    pthread_join(_thread, NULL);
}

void J9WallClock::timerLoop() {
    JNIEnv* jni = VM::attachThread("Async-profiler Sampler");
    jvmtiEnv* jvmti = VM::jvmti();

    int max_frames = _max_stack_depth + MAX_NATIVE_FRAMES + RESERVED_FRAMES;
    ASGCT_CallFrame* frames = (ASGCT_CallFrame*)malloc(max_frames * sizeof(ASGCT_CallFrame));

    while (_running) {
        if (!_enabled) {
            OS::sleep(_interval);
            continue;
        }

        jni->PushLocalFrame(64);

        jvmtiStackInfoExtended* stack_infos;
        jint thread_count;
        if (J9Ext::GetAllStackTracesExtended(_max_stack_depth, (void**)&stack_infos, &thread_count) == 0) {
            u64 start_time = TSC::ticks();
            for (int i = 0; i < thread_count; i++) {
                jvmtiStackInfoExtended* si = &stack_infos[i];
                for (int j = 0; j < si->frame_count; j++) {
                    jvmtiFrameInfoExtended* fi = &si->frame_buffer[j];
                    frames[j].method_id = fi->method;
                    frames[j].bci = FrameType::encode(fi->type, fi->location);
                }

                int tid = J9Ext::GetOSThreadID(si->thread);
                ExecutionEvent event(start_time);
                event._thread_state = (si->state & JVMTI_THREAD_STATE_RUNNABLE) ? THREAD_RUNNING : THREAD_SLEEPING;
                Profiler::instance()->recordExternalSample(_interval, tid, EXECUTION_SAMPLE, &event, si->frame_count, frames);
            }
            jvmti->Deallocate((unsigned char*)stack_infos);
        }

        jni->PopLocalFrame(NULL);

        OS::sleep(_interval);
    }

    free(frames);

    VM::detachThread();
}
