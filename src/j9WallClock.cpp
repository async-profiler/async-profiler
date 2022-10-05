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

#include <stdlib.h>
#include "j9WallClock.h"
#include "j9Ext.h"
#include "profiler.h"

volatile bool J9WallClock::_enabled = false;

long J9WallClock::_interval;

Error J9WallClock::start(Arguments& args) {
    if (_running) {
        // only one instance should be running
        return Error::OK;
    }

    if (args._wall >= 0 || strcmp(args._event, EVENT_WALL) == 0) {
        _sample_idle_threads = true;
    }
    int interval = args._event != NULL ? args._interval : args._wall;
    if (interval < 0) {
        return Error("interval must be non-negative");
    }

    _interval = interval ? interval : DEFAULT_WALL_INTERVAL;
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
            for (int i = 0; i < thread_count; i++) {
                jvmtiStackInfoExtended* si = &stack_infos[i];
                if (si->frame_count <= 0) {
                    // no frames recorded
                    continue;
                }
                ThreadState ts = (si->state & JVMTI_THREAD_STATE_RUNNABLE) ? THREAD_RUNNING : THREAD_SLEEPING;
                if (!_sample_idle_threads && ts != THREAD_RUNNING) {
                    // in execution profiler mode the non-running threads are skipped
                    continue;
                }
                for (int j = 0; j < si->frame_count; j++) {
                    jvmtiFrameInfoExtended* fi = &si->frame_buffer[j];
                    frames[j].method_id = fi->method;
                    frames[j].bci = FrameType::encode(fi->type, fi->location);
                }

                int tid = J9Ext::GetOSThreadID(si->thread);
                if (tid == -1) {
                    // clearly an invalid TID; skip the thread
                    continue;
                }
                ExecutionEvent event;
                event._thread_state = ts;
                event._context = Contexts::get(tid);
                if (ts == THREAD_RUNNING) {    
                    Profiler::instance()->recordExternalSample(_interval, tid, si->frame_count, frames, /*truncated=*/false, BCI_CPU, &event);
                }
                if (_sample_idle_threads) {
                    Profiler::instance()->recordExternalSample(_interval, tid, si->frame_count, frames, /*truncated=*/false, BCI_WALL, &event);
                }
            }
            jvmti->Deallocate((unsigned char*)stack_infos);
        }

        jni->PopLocalFrame(NULL);

        OS::sleep(_interval);
    }

    free(frames);

    VM::detachThread();
}
