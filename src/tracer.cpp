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

#include <set>
#include <errno.h>
#include <unistd.h>
#include "tracer.h"
#include "frameName.h"
#include "os.h"
#include "profiler.h"
#include "vmEntry.h"


struct TracerEvent {
    u64 timestamp;
    int tid;
    int call_trace_id;
    u64 counter;
};


Error Tracer::start(Arguments& args) {
    if (args._file == NULL || args._file[0] == 0) {
        return Error("Tracer output file is not specified");
    }

    _out = fopen(args._file, "w");
    if (_out == NULL) {
        return Error("Cannot open Tracer output file");
    }

    if (pipe(_pipefd) != 0) {
        fclose(_out);
        return Error("Unable to create poll pipe");
    }

    _simple = args._simple;
    _annotate = args._annotate;

    if (pthread_create(&_thread, NULL, threadEntry, this) != 0) {
        close(_pipefd[1]);
        close(_pipefd[0]);
        fclose(_out);
        return Error("Unable to create tracer thread");
    }

    _running = true;
    return Error::OK;
}

void Tracer::stop() {
    if (_running) {
        close(_pipefd[1]);
        pthread_join(_thread, NULL);
        close(_pipefd[0]);
        fclose(_out);
        _running = false;
    }
}

void Tracer::recordExecutionSample(int tid, int call_trace_id, u64 counter) {
    if (_running && tid != _tracer_tid && call_trace_id != 0) {
        TracerEvent event;
        event.timestamp = OS::millis();
        event.tid = tid;
        event.call_trace_id = call_trace_id;
        event.counter = counter;

        ssize_t result = write(_pipefd[1], &event, sizeof(event));
        (void)result;
    }
}

void Tracer::tracerLoop() {
    _tracer_tid = OS::threadId();
    TracerEvent event_buf[256];
    std::set<int> written_traces;

    void* env;
    JavaVMAttachArgs attach_args = {JNI_VERSION_1_6, (char*)"async-profiler tracer", NULL};
    VM::vm()->AttachCurrentThreadAsDaemon(&env, &attach_args);

    FrameName fn(_simple, _annotate, false,
                 Profiler::_instance._thread_names_lock, Profiler::_instance._thread_names);

    while (true) {
        ssize_t bytes = read(_pipefd[0], event_buf, sizeof(event_buf));
        if (bytes == 0 || (bytes < 0 && errno != EINTR)) {
            break;
        }

        for (TracerEvent* event = event_buf; (bytes -= sizeof(TracerEvent)) >= 0; event++) {
            int call_trace_id = event->call_trace_id;
            if (written_traces.insert(call_trace_id).second) {
                // The stacktrace with this id was not yet written
                CallTraceSample* trace = &Profiler::_instance._traces[call_trace_id];
                ASGCT_CallFrame* frame_buffer = &Profiler::_instance._frame_buffer[trace->_start_frame];

                fprintf(_out, "[stacktrace] id=%d ", call_trace_id);
                for (int i = trace->_num_frames - 1; i >= 0; i--) {
                    const char* frame_name = fn.name(frame_buffer[i]);
                    fprintf(_out, (i == 0 ? "%s" : "%s;"), frame_name);
                }
                fprintf(_out, "\n");
            }

            fprintf(_out, "[event] time=%llu thread=%d stacktrace=%d counter=%llu\n",
                    event->timestamp, event->tid, event->call_trace_id, event->counter);
            fflush(_out);
        }
    }

    VM::vm()->DetachCurrentThread();
}
