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
#include "jfrMetadata.h"
#include "dictionary.h"
#include "os.h"
#include "profiler.h"
#include "threadFilter.h"
#include "vmStructs.h"


const int RECORDING_BUFFER_SIZE = 65536;
const int RECORDING_LIMIT = RECORDING_BUFFER_SIZE - 4096;


enum FrameTypeId {
    FRAME_INTERPRETED  = 1,
    FRAME_JIT_COMPILED = 2,
    FRAME_INLINED      = 3,
    FRAME_NATIVE       = 4,
    FRAME_CPP          = 5,
    FRAME_KERNEL       = 6,
};


class MethodInfo {
  public:
    MethodInfo() : _key(0) {
    }

    u32 _key;
    u32 _class;
    u32 _name;
    u32 _sig;
    jint _modifiers;
    jint _line_number_table_size;
    jvmtiLineNumberEntry* _line_number_table;
    FrameTypeId _type;

    jint getLineNumber(jint bci) {
        if (_line_number_table_size == 0) {
            return 0;
        }

        int i = 1;
        while (i < _line_number_table_size && bci >= _line_number_table[i].start_location) {
            i++;
        }
        return _line_number_table[i - 1].line_number;
    }
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

    int skip(int delta) {
        int offset = _offset;
        _offset = offset + delta;
        return offset;
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

    void putVarint(u64 v) {
        char b = v;
        while ((v >>= 7) != 0) {
            _data[_offset++] = b | 0x80;
            b = v;
        }
        _data[_offset++] = b;
    }

    void putUtf8(const char* v) {
        putUtf8(v, strlen(v));
    }

    void putUtf8(const char* v, int len) {
        put8(3);
        putVarint(len);
        put(v, len);
    }

    void put8(int offset, char v) {
        _data[offset] = v;
    }

    void putVar32(int offset, u32 v) {
        _data[offset] = v | 0x80;
        _data[offset + 1] = (v >> 7) | 0x80;
        _data[offset + 2] = (v >> 14) | 0x80;
        _data[offset + 3] = (v >> 21) | 0x80;
        _data[offset + 4] = (v >> 28);
    }
};


class Recording {
  private:
    Buffer _buf[CONCURRENCY_LEVEL];
    int _fd;
    ThreadFilter _thread_set;
    Dictionary _packages;
    Dictionary _symbols;
    std::map<jmethodID, MethodInfo> _method_map;
    u64 _start_time;
    u64 _start_nanos;
    u64 _stop_time;
    u64 _stop_nanos;

  public:
    Recording(int fd) : _fd(fd), _thread_set(), _packages(), _symbols(), _method_map() {
        _start_time = OS::millis();
        _start_nanos = OS::nanotime();

        writeHeader(_buf);
        writeMetadata(_buf);
        flush(_buf);
    }

    ~Recording() {
        _stop_nanos = OS::nanotime();
        _stop_time = OS::millis();

        for (int i = 0; i < CONCURRENCY_LEVEL; i++) {
            flush(&_buf[i]);
        }

        off_t cpool_offset = lseek(_fd, 0, SEEK_CUR);
        writeCpool(_buf);
        flush(_buf);

        off_t chunk_size = lseek(_fd, 0, SEEK_CUR);

        // Patch checkpoint size field
        _buf->putVar32(0, chunk_size - cpool_offset);
        ssize_t result = pwrite(_fd, _buf->data(), 5, cpool_offset);
        (void)result;

        // Patch chunk header
        _buf->put64(chunk_size);
        _buf->put64(cpool_offset);
        _buf->put64(68);
        _buf->put64(_start_time * 1000000);
        _buf->put64(_stop_nanos - _start_nanos);
        result = pwrite(_fd, _buf->data(), 40, 8);
        (void)result;

        close(_fd);
    }

    Buffer* buffer(int lock_index) {
        return &_buf[lock_index];
    }

    void fillNativeMethodInfo(MethodInfo* mi, const char* name) {
        mi->_class = Profiler::_instance.classMap()->lookup("");
        mi->_modifiers = 0x100;
        mi->_line_number_table_size = 0;
        mi->_line_number_table = NULL;

        if (name[0] == '_' && name[1] == 'Z') {
            int status;
            char* demangled = abi::__cxa_demangle(name, NULL, NULL, &status);
            if (demangled != NULL) {
                char* p = strchr(demangled, '(');
                if (p != NULL) *p = 0;
                mi->_name = _symbols.lookup(demangled);
                mi->_sig = _symbols.lookup("()L;");
                mi->_type = FRAME_CPP;
                free(demangled);
                return;
            }
        }

        size_t len = strlen(name);
        if (len >= 4 && strcmp(name + len - 4, "_[k]") == 0) {
            mi->_name = _symbols.lookup(name, len - 4);
            mi->_sig = _symbols.lookup("(Lk;)L;");
            mi->_type = FRAME_KERNEL;
        } else {
            mi->_name = _symbols.lookup(name);
            mi->_sig = _symbols.lookup("()L;");
            mi->_type = FRAME_NATIVE;
        }
    }

    void fillJavaMethodInfo(MethodInfo* mi, jmethodID method) {
        jvmtiEnv* jvmti = VM::jvmti();
        jclass method_class;
        char* class_name = NULL;
        char* method_name = NULL;
        char* method_sig = NULL;

        if (jvmti->GetMethodDeclaringClass(method, &method_class) == 0 &&
            jvmti->GetClassSignature(method_class, &class_name, NULL) == 0 &&
            jvmti->GetMethodName(method, &method_name, &method_sig, NULL) == 0) {
            mi->_class = Profiler::_instance.classMap()->lookup(class_name + 1, strlen(class_name) - 2);
            mi->_name = _symbols.lookup(method_name);
            mi->_sig = _symbols.lookup(method_sig);
        } else {
            mi->_class = Profiler::_instance.classMap()->lookup("");
            mi->_name = _symbols.lookup("jvmtiError");
            mi->_sig = _symbols.lookup("()L;");
        }

        jvmti->Deallocate((unsigned char*)method_sig);
        jvmti->Deallocate((unsigned char*)method_name);
        jvmti->Deallocate((unsigned char*)class_name);

        if (jvmti->GetMethodModifiers(method, &mi->_modifiers) != 0) {
            mi->_modifiers = 0;
        }

        if (jvmti->GetLineNumberTable(method, &mi->_line_number_table_size, &mi->_line_number_table) != 0) {
            mi->_line_number_table_size = 0;
            mi->_line_number_table = NULL;
        }

        mi->_type = FRAME_INTERPRETED;
    }

    MethodInfo* resolveMethod(ASGCT_CallFrame& frame) {
        jmethodID method = frame.method_id;
        MethodInfo* mi = &_method_map[method];

        if (mi->_key == 0) {
            mi->_key = _method_map.size();

            if (method == NULL) {
                fillNativeMethodInfo(mi, "unknown");
            } else if (frame.bci == BCI_NATIVE_FRAME || frame.bci == BCI_ERROR) {
                fillNativeMethodInfo(mi, (const char*)method);
            } else {
                fillJavaMethodInfo(mi, method);
            }
        }

        return mi;
    }

    u32 getPackage(const char* class_name) {
        const char* package = strrchr(class_name, '/');
        if (package == NULL) {
            return 0;
        }
        if (class_name[0] == '[') {
            class_name = strchr(class_name, 'L') + 1;
        }
        return _packages.lookup(class_name, package - class_name);
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
        buf->put("FLR\0", 4);               // magic
        buf->put16(2);                      // major
        buf->put16(0);                      // minor
        buf->put64(0);                      // chunk size
        buf->put64(0);                      // cpool offset
        buf->put64(0);                      // meta offset
        buf->put64(_start_time * 1000000);  // start time, ns
        buf->put64(0);                      // duration, ns
        buf->put64(_start_nanos);           // start ticks
        buf->put64(1000000000);             // ticks per sec
        buf->put32(1);                      // features
    }

    void writeMetadata(Buffer* buf) {
        int metadata_start = buf->skip(5);  // size will be patched later
        buf->putVarint(T_METADATA);
        buf->putVarint(_start_nanos);
        buf->putVarint(0);
        buf->putVarint(1);

        std::vector<std::string>& strings = JfrMetadata::strings();
        buf->putVarint(strings.size());
        for (int i = 0; i < strings.size(); i++) {
            buf->putUtf8(strings[i].c_str());
        }

        writeElement(buf, JfrMetadata::root());

        buf->putVar32(metadata_start, buf->offset() - metadata_start);
    }

    void writeElement(Buffer* buf, const Element* e) {
        buf->putVarint(e->_name);

        buf->putVarint(e->_attributes.size());
        for (int i = 0; i < e->_attributes.size(); i++) {
            buf->putVarint(e->_attributes[i]._key);
            buf->putVarint(e->_attributes[i]._value);
        }

        buf->putVarint(e->_children.size());
        for (int i = 0; i < e->_children.size(); i++) {
            writeElement(buf, e->_children[i]);
        }
    }

    void writeCpool(Buffer* buf) {
        buf->skip(5);  // size will be patched later
        buf->putVarint(T_CPOOL);
        buf->putVarint(_start_nanos);
        buf->putVarint(0);
        buf->putVarint(0);
        buf->putVarint(1);

        buf->putVarint(8);

        writeFrameTypes(buf);
        writeThreadStates(buf);
        writeThreads(buf);
        writeStackTraces(buf);
        writeMethods(buf);
        writeClasses(buf);
        writePackages(buf);
        writeSymbols(buf);
    }

    void writeFrameTypes(Buffer* buf) {
        buf->putVarint(T_FRAME_TYPE);
        buf->putVarint(6);
        buf->putVarint(FRAME_INTERPRETED);  buf->putUtf8("Interpreted");
        buf->putVarint(FRAME_JIT_COMPILED); buf->putUtf8("JIT compiled");
        buf->putVarint(FRAME_INLINED);      buf->putUtf8("Inlined");
        buf->putVarint(FRAME_NATIVE);       buf->putUtf8("Native");
        buf->putVarint(FRAME_CPP);          buf->putUtf8("C++");
        buf->putVarint(FRAME_KERNEL);       buf->putUtf8("Kernel");
    }

    void writeThreadStates(Buffer* buf) {
        buf->putVarint(T_THREAD_STATE);
        buf->putVarint(2);
        buf->putVarint(THREAD_RUNNING);     buf->putUtf8("STATE_RUNNABLE");
        buf->putVarint(THREAD_SLEEPING);    buf->putUtf8("STATE_SLEEPING");
    }

    void writeThreads(Buffer* buf) {
        std::vector<int> threads;
        _thread_set.collect(threads);

        MutexLocker ml(Profiler::_instance._thread_names_lock);
        std::map<int, std::string>& thread_names = Profiler::_instance._thread_names;
        std::map<int, jlong>& thread_ids = Profiler::_instance._thread_ids;
        char name_buf[32];

        buf->putVarint(T_THREAD);
        buf->putVarint(threads.size());
        for (int i = 0; i < threads.size(); i++) {
            const char* thread_name;
            jlong thread_id;
            std::map<int, std::string>::const_iterator it = thread_names.find(threads[i]);
            if (it != thread_names.end()) {
                thread_name = it->second.c_str();
                thread_id = thread_ids[threads[i]];
            } else {
                sprintf(name_buf, "[tid=%d]", threads[i]);
                thread_name = name_buf;
                thread_id = 0;
            }

            buf->putVarint(threads[i]);
            buf->putUtf8(thread_name);
            buf->putVarint(threads[i]);
            if (thread_id == 0) {
                buf->put8(0);
            } else {
                buf->putUtf8(thread_name);
            }
            buf->putVarint(thread_id);
            flushIfNeeded(buf);
        }
    }

    void writeStackTraces(Buffer* buf) {
        std::map<u32, CallTrace*> traces;
        Profiler::_instance._call_trace_storage.collect(traces);

        buf->putVarint(T_STACK_TRACE);
        buf->putVarint(traces.size());
        for (std::map<u32, CallTrace*>::const_iterator it = traces.begin(); it != traces.end(); ++it) {
            CallTrace* trace = it->second;
            buf->putVarint(it->first);
            buf->putVarint(0);  // truncated
            buf->putVarint(trace->num_frames);
            for (int i = 0; i < trace->num_frames; i++) {
                MethodInfo* mi = resolveMethod(trace->frames[i]);
                buf->putVarint(mi->_key);
                jint bci = trace->frames[i].bci;
                if (bci >= 0) {
                    buf->putVarint(mi->getLineNumber(bci));
                    buf->putVarint(bci);
                } else {
                    buf->put8(0);
                    buf->put8(0);
                }
                buf->putVarint(mi->_type);
                flushIfNeeded(buf);
            }
            flushIfNeeded(buf);
        }
    }

    void writeMethods(Buffer* buf) {
        jvmtiEnv* jvmti = VM::jvmti();

        buf->putVarint(T_METHOD);
        buf->putVarint(_method_map.size());
        for (std::map<jmethodID, MethodInfo>::const_iterator it = _method_map.begin(); it != _method_map.end(); ++it) {
            const MethodInfo& mi = it->second;
            buf->putVarint(mi._key);
            buf->putVarint(mi._class);
            buf->putVarint(mi._name);
            buf->putVarint(mi._sig);
            buf->putVarint(mi._modifiers);
            buf->putVarint(0);  // hidden
            flushIfNeeded(buf);

            if (mi._line_number_table != NULL) {
                jvmti->Deallocate((unsigned char*)mi._line_number_table);
            }
        }
    }

    void writeClasses(Buffer* buf) {
        std::map<u32, const char*> classes;
        Profiler::_instance.classMap()->collect(classes);

        buf->putVarint(T_CLASS);
        buf->putVarint(classes.size());
        for (std::map<u32, const char*>::const_iterator it = classes.begin(); it != classes.end(); ++it) {
            const char* name = it->second;
            buf->putVarint(it->first);
            buf->putVarint(0);  // classLoader
            buf->putVarint(_symbols.lookup(name));
            buf->putVarint(getPackage(name));
            buf->putVarint(0);  // access flags
            flushIfNeeded(buf);
        }
    }

    void writePackages(Buffer* buf) {
        std::map<u32, const char*> packages;
        _packages.collect(packages);

        buf->putVarint(T_PACKAGE);
        buf->putVarint(packages.size());
        for (std::map<u32, const char*>::const_iterator it = packages.begin(); it != packages.end(); ++it) {
            buf->putVarint(it->first);
            buf->putVarint(_symbols.lookup(it->second));
            flushIfNeeded(buf);
        }
    }

    void writeSymbols(Buffer* buf) {
        std::map<u32, const char*> symbols;
        _symbols.collect(symbols);

        buf->putVarint(T_SYMBOL);
        buf->putVarint(symbols.size());
        for (std::map<u32, const char*>::const_iterator it = symbols.begin(); it != symbols.end(); ++it) {
            buf->putVarint(it->first);
            buf->putUtf8(it->second);
            flushIfNeeded(buf);
        }
    }

    void recordExecutionSample(Buffer* buf, int tid, u32 call_trace_id, ExecutionEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_EXECUTION_SAMPLE);
        buf->putVarint(OS::nanotime());
        buf->putVarint(tid);
        buf->putVarint(call_trace_id);
        buf->putVarint(event->_thread_state);
        buf->put8(start, buf->offset() - start);
    }

    void recordAllocationInNewTLAB(Buffer* buf, int tid, u32 call_trace_id, AllocEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_ALLOC_IN_NEW_TLAB);
        buf->putVarint(OS::nanotime());
        buf->putVarint(tid);
        buf->putVarint(call_trace_id);
        buf->putVarint(event->_class_id);
        buf->putVarint(event->_instance_size);
        buf->putVarint(event->_total_size);
        buf->put8(start, buf->offset() - start);
    }

    void recordAllocationOutsideTLAB(Buffer* buf, int tid, u32 call_trace_id, AllocEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_ALLOC_OUTSIDE_TLAB);
        buf->putVarint(OS::nanotime());
        buf->putVarint(tid);
        buf->putVarint(call_trace_id);
        buf->putVarint(event->_class_id);
        buf->putVarint(event->_total_size);
        buf->put8(start, buf->offset() - start);
    }

    void recordMonitorBlocked(Buffer* buf, int tid, u32 call_trace_id, LockEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_MONITOR_ENTER);
        buf->putVarint(event->_start_time);
        buf->putVarint(event->_end_time - event->_start_time);
        buf->putVarint(tid);
        buf->putVarint(call_trace_id);
        buf->putVarint(event->_class_id);
        buf->putVarint(event->_address);
        buf->put8(start, buf->offset() - start);
    }

    void recordThreadPark(Buffer* buf, int tid, u32 call_trace_id, LockEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_THREAD_PARK);
        buf->putVarint(event->_start_time);
        buf->putVarint(event->_end_time - event->_start_time);
        buf->putVarint(tid);
        buf->putVarint(call_trace_id);
        buf->putVarint(event->_class_id);
        buf->putVarint(event->_timeout);
        buf->putVarint(event->_address);
        buf->put8(start, buf->offset() - start);
    }

    void addThread(int tid) {
        if (!_thread_set.accept(tid)) {
            _thread_set.add(tid);
        }
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

// TODO: record events with call_trace_id == 0, and use stack allocated buffer if needed
void FlightRecorder::recordEvent(int lock_index, int tid, u32 call_trace_id,
                                 int event_type, Event* event, u64 counter) {
    if (_rec != NULL && call_trace_id != 0) {
        Buffer* buf = _rec->buffer(lock_index);
        switch (event_type) {
            case 0:
                _rec->recordExecutionSample(buf, tid, call_trace_id, (ExecutionEvent*)event);
                break;
            case BCI_ALLOC:
                _rec->recordAllocationInNewTLAB(buf, tid, call_trace_id, (AllocEvent*)event);
                break;
            case BCI_ALLOC_OUTSIDE_TLAB:
                _rec->recordAllocationOutsideTLAB(buf, tid, call_trace_id, (AllocEvent*)event);
                break;
            case BCI_LOCK:
                _rec->recordMonitorBlocked(buf, tid, call_trace_id, (LockEvent*)event);
                break;
            case BCI_PARK:
                _rec->recordThreadPark(buf, tid, call_trace_id, (LockEvent*)event);
                break;
        }
        _rec->flushIfNeeded(buf);
        _rec->addThread(tid);
    }
}
