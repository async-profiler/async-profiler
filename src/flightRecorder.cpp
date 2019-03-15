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

#include <map>
#include <string>
#include <arpa/inet.h>
#include <cxxabi.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "flightRecorder.h"
#include "os.h"
#include "profiler.h"
#include "vmStructs.h"


#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof(arr[0]))

const int RECORDING_BUFFER_SIZE = 65536;
const int RECORDING_LIMIT = RECORDING_BUFFER_SIZE - 4096;


enum DataType {
    T_BOOLEAN,
    T_BYTE,
    T_U1,
    T_SHORT,
    T_U2,
    T_INTEGER,
    T_U4,
    T_LONG,
    T_U8,
    T_FLOAT,
    T_DOUBLE,
    T_UTF8,
    T_STRING,
    T_ARRAY,
    T_STRUCT,
    T_STRUCTARRAY,
};

enum EventTypeId {
    EVENT_METADATA           = 0,
    EVENT_CHECKPOINT         = 1,
    EVENT_RECORDING          = 10,
    EVENT_RECORDING_SETTINGS = 11,
    EVENT_EXECUTION_SAMPLE   = 20,
};

enum ContentTypeId {
    CONTENT_NONE        = 0,
    CONTENT_MEMORY      = 1,
    CONTENT_EPOCHMILLIS = 2,
    CONTENT_MILLIS      = 3,
    CONTENT_NANOS       = 4,
    CONTENT_THREAD      = 7,
    CONTENT_STACKTRACE  = 9,
    CONTENT_CLASS       = 10,
    CONTENT_METHOD      = 32,
    CONTENT_SYMBOL      = 33,
    CONTENT_STATE       = 34,
    CONTENT_FRAME_TYPE  = 47,
};

enum FrameTypeId {
    FRAME_INTERPRETED  = 1,
    FRAME_JIT_COMPILED = 2,
    FRAME_INLINED      = 3,
    FRAME_NATIVE       = 4,
    FRAME_CPP          = 5,
    FRAME_KERNEL       = 6,
    FRAME_TOTAL_COUNT  = 6
};

enum ThreadStateId {
    STATE_RUNNABLE    = 1,
    STATE_TOTAL_COUNT = 1
};


struct DataStructure {
    const char* id;
    const char* name;
    DataType data_type;
    ContentTypeId content_type;
    int data_struct_index;
};

struct EventType {
    EventTypeId id;
    const char* name;
    const char* description;
    const char* path;
    bool has_start_time;
    bool has_thread;
    bool can_have_stacktrace;
    bool is_requestable;
    int data_structure;
};

struct ContentType {
    ContentTypeId id;
    const char* name;
    const char* description;
    DataType data_type;
    int data_structure;
};


const DataStructure
    ds_recording[] = {
        {"id", "Id", T_LONG},
        {"name", "Name", T_STRING},
        {"destination", "Destination", T_STRING},
        {"startTime", "Start Time", T_LONG, CONTENT_EPOCHMILLIS},
        {"duration", "Recording Duration", T_LONG, CONTENT_MILLIS},
        {"maxSize", "Max Size", T_LONG, CONTENT_MEMORY},
        {"maxAge", "Max Age", T_LONG, CONTENT_MILLIS},
    },
    ds_recording_settings[] = {
        {"id", "Id", T_INTEGER},
        {"name", "Name", T_STRING},
        {"path", "Event Path", T_STRING},
        {"enabled", "Enabled", T_BOOLEAN},
        {"stacktrace", "Stack Trace", T_BOOLEAN},
        {"period", "Period", T_LONG, CONTENT_MILLIS},
        {"threshold", "Threshold", T_LONG, CONTENT_NANOS},
    };

const EventType et_recording[] = {
    {EVENT_RECORDING, "Flight Recording", "", "recordings/recording", false, false, false, false, 0},
    {EVENT_RECORDING_SETTINGS, "Recording Setting", "", "recordings/recording_setting", false, false, false, true, 1},
};

const DataStructure
    ds_utf8[] = {
        {"utf8", "UTF8 data", T_UTF8},
    },
    ds_thread[] = {
        {"name", "Thread name", T_UTF8},
    },
    ds_frame_type[] = {
        {"desc", "Description", T_UTF8},
    },
    ds_state[] = {
        {"name", "Name", T_UTF8},
    },
    ds_class[] = {
        {"loaderClass", "ClassLoader", T_U8, CONTENT_CLASS},
        {"name", "Name", T_U8, CONTENT_SYMBOL},
        {"modifiers", "Access modifiers", T_SHORT},
    },
    ds_method[] = {
        {"class", "Class", T_U8, CONTENT_CLASS},
        {"name", "Name", T_U8, CONTENT_SYMBOL},
        {"signature", "Signature", T_U8, CONTENT_SYMBOL},
        {"modifiers", "Access modifiers", T_SHORT},
        {"hidden", "Hidden", T_BOOLEAN},
    },
    ds_frame[] = {
        {"method", "Java Method", T_U8, CONTENT_METHOD},
        {"bci", "Byte code index", T_INTEGER},
        {"type", "Frame type", T_U1, CONTENT_FRAME_TYPE},
    },
    ds_stacktrace[] = {
        {"truncated", "Truncated", T_BOOLEAN},
        {"frames", "Stack frames", T_STRUCTARRAY, CONTENT_NONE, /* ds_frame */ 6},
    },
    ds_method_sample[] = {
        {"sampledThread", "Thread", T_U4, CONTENT_THREAD},
        {"stackTrace", "Stack Trace", T_U8, CONTENT_STACKTRACE},
        {"state", "Thread State", T_U2, CONTENT_STATE},
    };

const EventType et_profile[] = {
    {EVENT_EXECUTION_SAMPLE, "Method Profiling Sample", "Snapshot of a threads state", "vm/prof/execution_sample", false, false, false, true, /* ds_method_sample */ 8},
};

const ContentType ct_profile[] = {
    {CONTENT_SYMBOL, "UTFConstant", "UTF constant", T_U8, 0},
    {CONTENT_THREAD, "Thread", "Thread", T_U4, 1},
    {CONTENT_FRAME_TYPE, "FrameType", "Frame type", T_U1, 2},
    {CONTENT_STATE, "ThreadState", "Java Thread State", T_U2, 3},
    {CONTENT_CLASS, "Class", "Java class", T_U8, 4},
    {CONTENT_METHOD, "Method", "Java method", T_U8, 5},
    {CONTENT_STACKTRACE, "StackTrace", "Stacktrace", T_U8, 7},
};


class MethodInfo {
  public:
    MethodInfo() : _key(0) {
    }

    int _key;
    int _class;
    int _name;
    int _sig;
    short _modifiers;
    FrameTypeId _type;
};


class Buffer {
  private:
    int _offset;
    char _data[RECORDING_BUFFER_SIZE - sizeof(int)];

  public:
    Buffer() : _offset(0) {
    }

    const char* data() const {
        return _data;
    }

    int offset() const {
        return _offset;
    }

    void reset() {
        _offset = 0;
    }

    void put(const char* v, int len) {
        memcpy(_data + _offset, v, len);
        _offset += len;
    }

    void put8(char v) {
        _data[_offset++] = v;
    }

    void put16(short v) {
        *(short*)(_data + _offset) = htons(v);
        _offset += 2;
    }

    void put32(int v) {
        *(int*)(_data + _offset) = htonl(v);
        _offset += 4;
    }

    void put64(u64 v) {
        *(u64*)(_data + _offset) = OS::hton64(v);
        _offset += 8;
    }

    void put32(int offset, int v) {
        *(int*)(_data + offset) = htonl(v);
    }

    void putUtf8(const char* v) {
        putUtf8(v, strlen(v));
    }

    void putUtf8(const char* v, int len) {
        put16((short)len);
        put(v, len);
    }

    void putUtf16(const char* v) {
        putUtf16(v, strlen(v));
    }

    void putUtf16(const char* v, int len) {
        put32(len);
        for (int i = 0; i < len; i++) {
            put16(v[i]);
        }
    }
};


class Recording {
  private:
    Buffer _buf[CONCURRENCY_LEVEL];
    int _fd;
    std::map<std::string, int> _symbol_map;
    std::map<std::string, int> _class_map;
    std::map<jmethodID, MethodInfo> _method_map;
    u64 _start_time;
    u64 _start_nanos;
    u64 _stop_time;
    u64 _stop_nanos;

  public:
    Recording(int fd) : _fd(fd), _symbol_map(), _class_map(), _method_map() {
        _start_time = OS::millis();
        _start_nanos = OS::nanotime();

        writeHeader(_buf);
        flush(_buf);
    }

    ~Recording() {
        _stop_nanos = OS::nanotime();
        _stop_time = OS::millis();

        for (int i = 0; i < CONCURRENCY_LEVEL; i++) {
            flush(&_buf[i]);
        }

        writeRecordingInfo(_buf);
        flush(_buf);

        off_t checkpoint_offset = lseek(_fd, 0, SEEK_CUR);
        writeCheckpoint(_buf);
        flush(_buf);

        off_t metadata_offset = lseek(_fd, 0, SEEK_CUR);
        writeMetadata(_buf, checkpoint_offset);
        flush(_buf);

        // Patch checkpoint size field
        int checkpoint_size = htonl((int)(metadata_offset - checkpoint_offset));
        ssize_t result = pwrite(_fd, &checkpoint_size, sizeof(checkpoint_size), checkpoint_offset);
        (void)result;

        // Patch metadata offset
        u64 metadata_start = OS::hton64(metadata_offset);
        result = pwrite(_fd, &metadata_start, sizeof(metadata_start), 8);
        (void)result;

        close(_fd);
    }

    int lookup(std::map<std::string, int>& map, std::string key) {
        int* value = &map[key];
        if (*value == 0) *value = map.size();
        return *value;
    }

    FrameTypeId demangle(const char* name, std::string& result) {
        if (name == NULL) {
            result = "unknown";
            return FRAME_NATIVE;
        }

        if (name[0] == '_' && name[1] == 'Z') {
            int status;
            char* demangled = abi::__cxa_demangle(name, NULL, NULL, &status);
            if (demangled != NULL) {
                char* p = strchr(demangled, '(');
                if (p != NULL) *p = 0;
                result = demangled;
                free(demangled);
                return FRAME_CPP;
            }
        }

        int len = strlen(name);
        if (len >= 4 && strcmp(name + len - 4, "_[k]") == 0) {
            result = std::string(name, len - 4);
            return FRAME_KERNEL;
        }

        result = name;
        return FRAME_NATIVE;
    }

    MethodInfo* resolveMethod(ASGCT_CallFrame& frame) {
        jmethodID method = frame.method_id;
        MethodInfo* mi = &_method_map[method];

        if (mi->_key == 0) {
            mi->_key = _method_map.size();

            if (frame.bci == BCI_NATIVE_FRAME || frame.bci == BCI_KERNEL_FRAME || frame.bci == BCI_ERROR || method == NULL) {
                std::string name;
                FrameTypeId type = demangle((const char*)method, name);
                mi->_class = lookup(_class_map, "");
                mi->_name = lookup(_symbol_map, name);
                mi->_sig = lookup(_symbol_map, frame.bci == BCI_KERNEL_FRAME ? "(Lk;)L;" : "()L;");
                mi->_modifiers = 0x100;
                mi->_type = type;

            } else if (frame.bci == BCI_SYMBOL || frame.bci == BCI_SYMBOL_OUTSIDE_TLAB) {
                VMSymbol* symbol = (VMSymbol*)((intptr_t)method & ~1);
                mi->_class = lookup(_class_map, std::string(symbol->body(), symbol->length()));
                mi->_name = lookup(_symbol_map, "new");
                mi->_sig = lookup(_symbol_map, "()L;");
                mi->_modifiers = 0x100;
                mi->_type = FRAME_NATIVE;

            } else {
                jvmtiEnv* jvmti = VM::jvmti();
                jclass method_class;
                char* class_name = NULL;
                char* method_name = NULL;
                char* method_sig = NULL;
                int modifiers = 0;

                if (jvmti->GetMethodDeclaringClass(method, &method_class) == 0 &&
                    jvmti->GetClassSignature(method_class, &class_name, NULL) == 0 &&
                    jvmti->GetMethodName(method, &method_name, &method_sig, NULL) == 0) {
                    jvmti->GetMethodModifiers(method, &modifiers);
                    mi->_class = lookup(_class_map, std::string(class_name + 1, strlen(class_name) - 2));
                    mi->_name = lookup(_symbol_map, method_name);
                    mi->_sig = lookup(_symbol_map, method_sig);
                } else {
                    mi->_class = lookup(_class_map, "");
                    mi->_name = lookup(_symbol_map, "jvmtiError");
                    mi->_sig = lookup(_symbol_map, "()L;");
                }

                mi->_modifiers = (short)modifiers;
                mi->_type = FRAME_INTERPRETED;

                jvmti->Deallocate((unsigned char*)method_sig);
                jvmti->Deallocate((unsigned char*)method_name);
                jvmti->Deallocate((unsigned char*)class_name);
            }
        }

        return mi;
    }

    void flush(Buffer* buf) {
        ssize_t result = write(_fd, buf->data(), buf->offset());
        (void)result;
        buf->reset();
    }

    void flushIfNeeded(Buffer* buf) {
        if (buf->offset() >= RECORDING_LIMIT) {
            flush(buf);
        }
    }

    void writeHeader(Buffer* buf) {
        buf->put("FLR\0", 4);  // magic
        buf->put16(0);         // major
        buf->put16(9);         // minor
        buf->put64(0);         // metadata offset
    }

    void writeRecordingInfo(Buffer* buf) {
        int recording_start = buf->offset();
        buf->put32(0);  // size
        buf->put32(EVENT_RECORDING);
        buf->put64(_stop_nanos);
        buf->put64(1);  // id
        buf->putUtf16("Async-profiler");
        buf->putUtf16("async-profiler.jfr");
        buf->put64(_start_time);
        buf->put64(_stop_time - _start_time);
        buf->put64(0x7fffffff);
        buf->put64(0x7fffffff);
        buf->put32(recording_start, buf->offset() - recording_start);

        int recording_settings_start = buf->offset();
        buf->put32(0);  // size
        buf->put32(EVENT_RECORDING_SETTINGS);
        buf->put64(_stop_nanos);
        buf->put32(1);  // id
        buf->putUtf16("Method Profiling Sample");
        buf->putUtf16("vm/prof/execution_sample");
        buf->put8(1);
        buf->put8(0);
        buf->put64(1);
        buf->put64(0);
        buf->put32(recording_settings_start, buf->offset() - recording_settings_start);
    }

    void writeFixedTables(Buffer* buf) {
        // Frame types
        buf->put32(CONTENT_FRAME_TYPE);
        buf->put32(FRAME_TOTAL_COUNT);
        buf->put8(FRAME_INTERPRETED);  buf->putUtf8("Interpreted");
        buf->put8(FRAME_JIT_COMPILED); buf->putUtf8("JIT compiled");
        buf->put8(FRAME_INLINED);      buf->putUtf8("Inlined");
        buf->put8(FRAME_NATIVE);       buf->putUtf8("Native");
        buf->put8(FRAME_CPP);          buf->putUtf8("C++");
        buf->put8(FRAME_KERNEL);       buf->putUtf8("Kernel");

        // Thread states
        buf->put32(CONTENT_STATE);
        buf->put32(STATE_TOTAL_COUNT);
        buf->put16(STATE_RUNNABLE);    buf->putUtf8("STATE_RUNNABLE");
    }

    void writeStackTraces(Buffer* buf) {
        CallTraceSample* traces = Profiler::_instance._traces;
        ASGCT_CallFrame* frame_buffer = Profiler::_instance._frame_buffer;

        int count = 0;
        for (int i = 0; i < MAX_CALLTRACES; i++) {
            if (traces[i]._samples != 0) count++;
        }

        buf->put32(CONTENT_STACKTRACE);
        buf->put32(count);
        for (int i = 0; i < MAX_CALLTRACES; i++) {
            CallTraceSample& trace = traces[i];
            if (trace._samples != 0) {
                buf->put64(i);  // stack trace key
                buf->put8(0);   // truncated
                buf->put32(trace._num_frames);
                for (int j = 0; j < trace._num_frames; j++) {
                    MethodInfo* mi = resolveMethod(frame_buffer[trace._start_frame + j]);
                    buf->put64(mi->_key);  // method key
                    buf->put32(0);         // bci
                    buf->put8(mi->_type);  // frame type
                    flushIfNeeded(buf);
                }
                flushIfNeeded(buf);
            }
        }
    }

    void writeMethods(Buffer* buf) {
        buf->put32(CONTENT_METHOD);
        buf->put32(_method_map.size());
        for (std::map<jmethodID, MethodInfo>::const_iterator it = _method_map.begin(); it != _method_map.end(); ++it) {
            const MethodInfo& mi = it->second;
            buf->put64(mi._key);
            buf->put64(mi._class);
            buf->put64(mi._name);
            buf->put64(mi._sig);
            buf->put16(mi._modifiers);
            buf->put8(0);   // hidden
            flushIfNeeded(buf);
        }
    }

    void writeClasses(Buffer* buf) {
        buf->put32(CONTENT_CLASS);
        buf->put32(_class_map.size());
        for (std::map<std::string, int>::const_iterator it = _class_map.begin(); it != _class_map.end(); ++it) {
            buf->put64(it->second);
            buf->put64(0);  // loader class
            buf->put64(lookup(_symbol_map, it->first));
            buf->put16(0);  // access flags
            flushIfNeeded(buf);
        }
    }

    void writeSymbols(Buffer* buf) {
        buf->put32(CONTENT_SYMBOL);
        buf->put32(_symbol_map.size());
        for (std::map<std::string, int>::const_iterator it = _symbol_map.begin(); it != _symbol_map.end(); ++it) {
            buf->put64(it->second);
            buf->putUtf8(it->first.c_str());
            flushIfNeeded(buf);
        }
    }

    void writeThreads(Buffer* buf) {
        buf->put32(CONTENT_THREAD);
        buf->put32(0);
    }

    void writeCheckpoint(Buffer* buf) {
        buf->put32(0);   // size will be patched later
        buf->put32(EVENT_CHECKPOINT);
        buf->put64(_stop_nanos);
        buf->put64(0);   // previous checkpoint

        writeFixedTables(buf);
        writeStackTraces(buf);
        writeMethods(buf);
        writeClasses(buf);
        writeSymbols(buf);
        writeThreads(buf);
    }

    void writeDataStructure(Buffer* buf, int count, const DataStructure* ds) {
        buf->put32(count);
        for (int i = 0; i < count; i++, ds++) {
            buf->putUtf8(ds->id);
            buf->putUtf8(ds->name);
            buf->putUtf8("");
            buf->put8(0);
            buf->put8(ds->data_type);
            buf->put32(ds->content_type);
            buf->put32(ds->data_struct_index);
            buf->put32(0);
        }
    }

    void writeEventTypes(Buffer* buf, int count, const EventType* et) {
        buf->put32(count);
        for (int i = 0; i < count; i++, et++) {
            buf->put32(et->id);
            buf->putUtf8(et->name);
            buf->putUtf8(et->description);
            buf->putUtf8(et->path);
            buf->put8(et->has_start_time ? 1 : 0);
            buf->put8(et->has_thread ? 1 : 0);
            buf->put8(et->can_have_stacktrace ? 1 : 0);
            buf->put8(et->is_requestable ? 1 : 0);
            buf->put32(et->data_structure);
            buf->put32(0);
        }
    }

    void writeContentTypes(Buffer* buf, int count, const ContentType* ct) {
        buf->put32(count);
        for (int i = 0; i < count; i++, ct++) {
            buf->put32(ct->id);
            buf->putUtf8(ct->name);
            buf->putUtf8(ct->description);
            buf->put8(ct->data_type);
            buf->put32(ct->data_structure);
        }
    }

    void writeRecordingMetadata(Buffer* buf) {
        buf->put32(1);
        buf->putUtf8("JFR Metadata");
        buf->putUtf8("Information about Recordings and Settings");
        buf->putUtf8("http://www.oracle.com/hotspot/jfr-info/");

        // Relations
        buf->put32(0);

        // Data structures
        buf->put32(2);
        writeDataStructure(buf, ARRAY_SIZE(ds_recording), ds_recording);
        writeDataStructure(buf, ARRAY_SIZE(ds_recording_settings), ds_recording_settings);

        // Event types and content types
        writeEventTypes(buf, ARRAY_SIZE(et_recording), et_recording);
        writeContentTypes(buf, 0, NULL);
    }

    void writeProfileMetadata(Buffer* buf) {
        buf->put32(2);
        buf->putUtf8("HotSpot JVM");
        buf->putUtf8("Oracle Hotspot JVM");
        buf->putUtf8("http://www.oracle.com/hotspot/jvm/");

        // Relations
        buf->put32(0);

        // Data structures
        buf->put32(9);
        writeDataStructure(buf, ARRAY_SIZE(ds_utf8), ds_utf8);
        writeDataStructure(buf, ARRAY_SIZE(ds_thread), ds_thread);
        writeDataStructure(buf, ARRAY_SIZE(ds_frame_type), ds_frame_type);
        writeDataStructure(buf, ARRAY_SIZE(ds_state), ds_state);
        writeDataStructure(buf, ARRAY_SIZE(ds_class), ds_class);
        writeDataStructure(buf, ARRAY_SIZE(ds_method), ds_method);
        writeDataStructure(buf, ARRAY_SIZE(ds_frame), ds_frame);
        writeDataStructure(buf, ARRAY_SIZE(ds_stacktrace), ds_stacktrace);
        writeDataStructure(buf, ARRAY_SIZE(ds_method_sample), ds_method_sample);

        // Event types and content types
        writeEventTypes(buf, ARRAY_SIZE(et_profile), et_profile);
        writeContentTypes(buf, ARRAY_SIZE(ct_profile), ct_profile);
    }

    void writeMetadata(Buffer* buf, off_t checkpoint_offset) {
        int metadata_start = buf->offset();
        buf->put32(0);
        buf->put32(EVENT_METADATA);

        // Producers
        buf->put32(2);
        writeRecordingMetadata(buf);
        writeProfileMetadata(buf);

        buf->put64(_start_time);
        buf->put64(_stop_time);
        buf->put64(_start_nanos);
        buf->put64(1000000000);  // ticks per second
        buf->put64(checkpoint_offset);

        buf->put32(metadata_start, buf->offset() - metadata_start);
    }

    void recordExecutionSample(int lock_index, int tid, int call_trace_id) {
        Buffer* buf = &_buf[lock_index];
        buf->put32(30);
        buf->put32(EVENT_EXECUTION_SAMPLE);
        buf->put64(OS::nanotime());
        buf->put32(tid);
        buf->put64(call_trace_id);
        buf->put16(STATE_RUNNABLE);
        flushIfNeeded(buf);
    }
};


Error FlightRecorder::start(const char* file) {
    if (file == NULL || file[0] == 0) {
        return Error("Flight Recorder output file is not specified");
    }

    int fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        return Error("Cannot open Flight Recorder output file");
    }

    _rec = new Recording(fd);
    return Error::OK;
}

void FlightRecorder::stop() {
    if (_rec != NULL) {
        delete _rec;
        _rec = NULL;
    }
}

void FlightRecorder::recordExecutionSample(int lock_index, int tid, int call_trace_id) {
    if (_rec != NULL && call_trace_id != 0) {
        _rec->recordExecutionSample(lock_index, tid, call_trace_id);
    }
}
