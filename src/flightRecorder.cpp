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
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "flightRecorder.h"
#include "jfrMetadata.h"
#include "dictionary.h"
#include "os.h"
#include "profiler.h"
#include "symbols.h"
#include "threadFilter.h"
#include "vmStructs.h"


static const unsigned char JFR_COMBINER_CLASS[] = {
#include "helper/one/profiler/JfrCombiner.class.h"
};


const int BUFFER_SIZE = 1024;
const int BUFFER_LIMIT = BUFFER_SIZE - 128;
const int RECORDING_BUFFER_SIZE = 65536;
const int RECORDING_BUFFER_LIMIT = RECORDING_BUFFER_SIZE - 4096;
const int MAX_STRING_LENGTH = 8191;


static const char* const SETTING_RING[] = {NULL, "kernel", "user"};
static const char* const SETTING_CSTACK[] = {NULL, "no", "fp", "lbr"};


enum FrameTypeId {
    FRAME_INTERPRETED  = 0,
    FRAME_JIT_COMPILED = 1,
    FRAME_INLINED      = 2,
    FRAME_NATIVE       = 3,
    FRAME_CPP          = 4,
    FRAME_KERNEL       = 5,
};


struct CpuTime {
    u64 real;
    u64 user;
    u64 system;
};

struct CpuTimes {
    CpuTime proc;
    CpuTime total;
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

class Lookup {
  public:
    std::map<jmethodID, MethodInfo> _method_map;
    Dictionary* _classes;
    Dictionary _packages;
    Dictionary _symbols;

  private:
    void fillNativeMethodInfo(MethodInfo* mi, const char* name) {
        mi->_class = _classes->lookup("");
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
            mi->_class = _classes->lookup(class_name + 1, strlen(class_name) - 2);
            mi->_name = _symbols.lookup(method_name);
            mi->_sig = _symbols.lookup(method_sig);
        } else {
            mi->_class = _classes->lookup("");
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

  public:
    Lookup() : _method_map(), _packages(), _symbols() {
        _classes = Profiler::instance()->classMap();
    }

    ~Lookup() {
        jvmtiEnv* jvmti = VM::jvmti();

        for (std::map<jmethodID, MethodInfo>::const_iterator it = _method_map.begin(); it != _method_map.end(); ++it) {
            jvmtiLineNumberEntry* line_number_table = it->second._line_number_table;
            if (line_number_table != NULL) {
                jvmti->Deallocate((unsigned char*)line_number_table);
            }
        }
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

    u32 getSymbol(const char* name) {
        return _symbols.lookup(name);
    }
};


class Buffer {
  private:
    int _offset;
    char _data[BUFFER_SIZE - sizeof(int)];

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

    void put(const char* v, u32 len) {
        memcpy(_data + _offset, v, len);
        _offset += (int)len;
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

    void putFloat(float v) {
        union {
            float f;
            int i;
        } u;

        u.f = v;
        put32(u.i);
    }

    void putVar32(u32 v) {
        while (v > 0x7f) {
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
        }
        _data[_offset++] = (char)v;
    }

    void putVar64(u64 v) {
        int iter = 0;
        while (v > 0x1fffff) {
            _data[_offset++] = (char)v | 0x80; v >>= 7;
            _data[_offset++] = (char)v | 0x80; v >>= 7;
            _data[_offset++] = (char)v | 0x80; v >>= 7;
            if (++iter == 3) return;
        }
        while (v > 0x7f) {
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
        }
        _data[_offset++] = (char)v;
    }

    void putUtf8(const char* v) {
        if (v == NULL) {
            put8(0);
        } else {
            size_t len = strlen(v);
            putUtf8(v, len < MAX_STRING_LENGTH ? len : MAX_STRING_LENGTH);
        }
    }

    void putUtf8(const char* v, u32 len) {
        put8(3);
        putVar32(len);
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

class RecordingBuffer : public Buffer {
  private:
    char _buf[RECORDING_BUFFER_SIZE - sizeof(Buffer)];

  public:
    RecordingBuffer() : Buffer() {
    }
};


class Recording {
  private:
    static SpinLock _cpu_monitor_lock;
    static int _append_fd;

    static char* _agent_properties;
    static char* _jvm_args;
    static char* _jvm_flags;
    static char* _java_command;

    RecordingBuffer _buf[CONCURRENCY_LEVEL];
    int _fd;
    off_t _chunk_start;
    ThreadFilter _thread_set;
    u64 _start_time;
    u64 _start_nanos;
    u64 _stop_time;
    u64 _stop_nanos;
    int _tid;
    int _available_processors;
    int _recorded_lib_count;
    Buffer _cpu_monitor_buf;
    Timer* _cpu_monitor;
    CpuTimes _last_times;

    Timer* createCpuMonitor() {
        _last_times.proc.real = OS::getProcessCpuTime(&_last_times.proc.user, &_last_times.proc.system);
        _last_times.total.real = OS::getTotalCpuTime(&_last_times.total.user, &_last_times.total.system);

        return OS::startTimer(1000000000, cpuMonitorCallback, this);
    }

    void cpuMonitorCycle() {
        CpuTimes times;
        times.proc.real = OS::getProcessCpuTime(&times.proc.user, &times.proc.system);
        times.total.real = OS::getTotalCpuTime(&times.total.user, &times.total.system);

        float proc_user = 0, proc_system = 0, machine_total = 0;

        if (times.proc.real != (u64)-1 && times.proc.real > _last_times.proc.real) {
            float delta = (times.proc.real - _last_times.proc.real) * _available_processors;
            proc_user = ratio((times.proc.user - _last_times.proc.user) / delta);
            proc_system = ratio((times.proc.system - _last_times.proc.system) / delta);
        }

        if (times.total.real != (u64)-1 && times.total.real > _last_times.total.real) {
            float delta = times.total.real - _last_times.total.real;
            machine_total = ratio(((times.total.user + times.total.system) -
                                   (_last_times.total.user + _last_times.total.system)) / delta);
            if (machine_total < proc_user + proc_system) {
                machine_total = ratio(proc_user + proc_system);
            }
        }

        recordCpuLoad(&_cpu_monitor_buf, proc_user, proc_system, machine_total);
        flushIfNeeded(&_cpu_monitor_buf, BUFFER_LIMIT);

        _last_times = times;
    }

    static void cpuMonitorCallback(void* arg) {
        if (_cpu_monitor_lock.tryLock()) {
            ((Recording*)arg)->cpuMonitorCycle();
            _cpu_monitor_lock.unlock();
        }
    }

    static float ratio(float value) {
        return value < 0 ? 0 : value > 1 ? 1 : value;
    }

  public:
    Recording(int fd, Arguments& args) : _fd(fd), _thread_set() {
        _chunk_start = lseek(_fd, 0, SEEK_END);
        _start_time = OS::millis();
        _start_nanos = OS::nanotime();
        _tid = OS::threadId();
        addThread(_tid);
        VM::jvmti()->GetAvailableProcessors(&_available_processors);

        writeHeader(_buf);
        writeMetadata(_buf);
        writeRecordingInfo(_buf);
        writeSettings(_buf, args);
        if (!args.hasOption(NO_SYSTEM_INFO)) {
            writeOsCpuInfo(_buf);
            writeJvmInfo(_buf);
        }
        if (!args.hasOption(NO_SYSTEM_PROPS)) {
            writeSystemProperties(_buf);
        }
        if (!args.hasOption(NO_NATIVE_LIBS)) {
            _recorded_lib_count = 0;
            writeNativeLibraries(_buf);
        } else {
            _recorded_lib_count = -1;
        }
        flush(_buf);

        _cpu_monitor = !args.hasOption(NO_CPU_LOAD) ? createCpuMonitor() : NULL;
        _cpu_monitor_lock.unlock();
    }

    ~Recording() {
        off_t chunk_end = finishChunk(true);

        if (_append_fd >= 0) {
            OS::copyFile(_fd, _append_fd, 0, chunk_end);
        }

        close(_fd);
    }

    off_t finishChunk(bool last) {
        _cpu_monitor_lock.lock();
        if (last && _cpu_monitor != NULL) {
            OS::stopTimer(_cpu_monitor);
        }
        flush(&_cpu_monitor_buf);

        writeNativeLibraries(_buf);

        for (int i = 0; i < CONCURRENCY_LEVEL; i++) {
            flush(&_buf[i]);
        }

        _stop_nanos = OS::nanotime();
        _stop_time = OS::millis();

        off_t cpool_offset = lseek(_fd, 0, SEEK_CUR);
        writeCpool(_buf);
        flush(_buf);

        off_t chunk_end = lseek(_fd, 0, SEEK_CUR);

        // Patch cpool size field
        _buf->putVar32(0, chunk_end - cpool_offset);
        ssize_t result = pwrite(_fd, _buf->data(), 5, cpool_offset);
        (void)result;

        // Patch chunk header
        _buf->put64(chunk_end - _chunk_start);
        _buf->put64(cpool_offset - _chunk_start);
        _buf->put64(68);
        _buf->put64(_start_time * 1000000);
        _buf->put64(_stop_nanos - _start_nanos);
        result = pwrite(_fd, _buf->data(), 40, _chunk_start + 8);
        (void)result;

        _buf->reset();
        return chunk_end;
    }

    void switchChunk() {
        _chunk_start = finishChunk(false);
        _start_time = _stop_time;
        _start_nanos = _stop_nanos;

        writeHeader(_buf);
        writeMetadata(_buf);
        writeRecordingInfo(_buf);
        flush(_buf);

        _cpu_monitor_lock.unlock();
    }

    static void JNICALL appendRecording(JNIEnv* env, jclass cls, jstring file_name) {
        const char* file_name_str = env->GetStringUTFChars(file_name, NULL);
        if (file_name_str == NULL) {
            return;
        }

        _append_fd = open(file_name_str, O_WRONLY);
        if (_append_fd >= 0) {
            lseek(_append_fd, 0, SEEK_END);
            Profiler::instance()->stop();
            close(_append_fd);
            _append_fd = -1;
        } else {
            Log::warn("Failed to open JFR recording at %s: %s", file_name_str, strerror(errno));
        }

        env->ReleaseStringUTFChars(file_name, file_name_str);
    }

    Buffer* buffer(int lock_index) {
        return &_buf[lock_index];
    }

    bool parseAgentProperties() {
        JNIEnv* env = VM::jni();
        jclass vm_support = env->FindClass("jdk/internal/vm/VMSupport");
        if (vm_support == NULL) vm_support = env->FindClass("sun/misc/VMSupport");
        if (vm_support != NULL) {
            jmethodID get_agent_props = env->GetStaticMethodID(vm_support, "getAgentProperties", "()Ljava/util/Properties;");
            jmethodID to_string = env->GetMethodID(env->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");
            if (get_agent_props != NULL && to_string != NULL) {
                jobject props = env->CallStaticObjectMethod(vm_support, get_agent_props);
                if (props != NULL) {
                    jstring str = (jstring)env->CallObjectMethod(props, to_string);
                    if (str != NULL) {
                        _agent_properties = (char*)env->GetStringUTFChars(str, NULL);
                    }
                }
            }
        }
        env->ExceptionClear();

        if (_agent_properties == NULL) {
            return false;
        }

        char* p = _agent_properties + 1;
        p[strlen(p) - 1] = 0;

        while (*p) {
            if (strncmp(p, "sun.jvm.args=", 13) == 0) {
                _jvm_args = p + 13;
            } else if (strncmp(p, "sun.jvm.flags=", 14) == 0) {
                _jvm_flags = p + 14;
            } else if (strncmp(p, "sun.java.command=", 17) == 0) {
                _java_command = p + 17;
            }

            if ((p = strstr(p, ", ")) == NULL) {
                break;
            }

            *p = 0;
            p += 2;
        }

        return true;
    }

    void flush(Buffer* buf) {
        ssize_t result = write(_fd, buf->data(), buf->offset());
        (void)result;
        buf->reset();
    }

    void flushIfNeeded(Buffer* buf, int limit = RECORDING_BUFFER_LIMIT) {
        if (buf->offset() >= limit) {
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
        buf->putVar32(T_METADATA);
        buf->putVar64(_start_nanos);
        buf->putVar32(0);
        buf->putVar32(1);

        std::vector<std::string>& strings = JfrMetadata::strings();
        buf->putVar32(strings.size());
        for (int i = 0; i < strings.size(); i++) {
            buf->putUtf8(strings[i].c_str());
        }

        writeElement(buf, JfrMetadata::root());

        buf->putVar32(metadata_start, buf->offset() - metadata_start);
    }

    void writeElement(Buffer* buf, const Element* e) {
        buf->putVar32(e->_name);

        buf->putVar32(e->_attributes.size());
        for (int i = 0; i < e->_attributes.size(); i++) {
            buf->putVar32(e->_attributes[i]._key);
            buf->putVar32(e->_attributes[i]._value);
        }

        buf->putVar32(e->_children.size());
        for (int i = 0; i < e->_children.size(); i++) {
            writeElement(buf, e->_children[i]);
        }
    }

    void writeRecordingInfo(Buffer* buf) {
        int start = buf->skip(1);
        buf->put8(T_ACTIVE_RECORDING);
        buf->putVar64(_start_nanos);
        buf->putVar32(0);
        buf->putVar32(_tid);
        buf->putVar32(1);
        buf->putUtf8("async-profiler " PROFILER_VERSION);
        buf->putUtf8("async-profiler.jfr");
        buf->putVar64(0x7fffffffffffffffULL);
        buf->putVar32(0);
        buf->putVar64(_start_time);
        buf->putVar64(0x7fffffffffffffffULL);
        buf->put8(start, buf->offset() - start);
    }

    void writeSettings(Buffer* buf, Arguments& args) {
        writeStringSetting(buf, T_ACTIVE_RECORDING, "version", PROFILER_VERSION);
        writeStringSetting(buf, T_ACTIVE_RECORDING, "ring", SETTING_RING[args._ring]);
        writeStringSetting(buf, T_ACTIVE_RECORDING, "cstack", SETTING_CSTACK[args._cstack]);
        writeStringSetting(buf, T_ACTIVE_RECORDING, "event", args._event);
        writeStringSetting(buf, T_ACTIVE_RECORDING, "filter", args._filter);
        writeStringSetting(buf, T_ACTIVE_RECORDING, "begin", args._begin);
        writeStringSetting(buf, T_ACTIVE_RECORDING, "end", args._end);
        writeListSetting(buf, T_ACTIVE_RECORDING, "include", args._buf, args._include);
        writeListSetting(buf, T_ACTIVE_RECORDING, "exclude", args._buf, args._exclude);
        writeIntSetting(buf, T_ACTIVE_RECORDING, "jstackdepth", args._jstackdepth);
        writeIntSetting(buf, T_ACTIVE_RECORDING, "safemode", args._safe_mode);
        writeIntSetting(buf, T_ACTIVE_RECORDING, "jfropts", args._jfr_options);

        writeBoolSetting(buf, T_EXECUTION_SAMPLE, "enabled", args._event != NULL);
        if (args._event != NULL) {
            writeIntSetting(buf, T_EXECUTION_SAMPLE, "interval", args._interval);
        }

        writeBoolSetting(buf, T_ALLOC_IN_NEW_TLAB, "enabled", args._alloc > 0);
        writeBoolSetting(buf, T_ALLOC_OUTSIDE_TLAB, "enabled", args._alloc > 0);
        if (args._alloc > 0) {
            writeIntSetting(buf, T_ALLOC_IN_NEW_TLAB, "alloc", args._alloc);
        }

        writeBoolSetting(buf, T_MONITOR_ENTER, "enabled", args._lock > 0);
        writeBoolSetting(buf, T_THREAD_PARK, "enabled", args._lock > 0);
        if (args._lock > 0) {
            writeIntSetting(buf, T_MONITOR_ENTER, "lock", args._lock);
        }

        writeBoolSetting(buf, T_ACTIVE_RECORDING, "debugSymbols", VMStructs::hasDebugSymbols());
        writeBoolSetting(buf, T_ACTIVE_RECORDING, "kernelSymbols", Symbols::haveKernelSymbols());
        writeBoolSetting(buf, T_ACTIVE_RECORDING, "loadLibraryHook", Profiler::instance()->_original_NativeLibrary_load != NULL);
    }

    void writeStringSetting(Buffer* buf, int category, const char* key, const char* value) {
        int start = buf->skip(5);
        buf->put8(T_ACTIVE_SETTING);
        buf->putVar64(_start_nanos);
        buf->putVar32(0);
        buf->putVar32(_tid);
        buf->putVar32(category);
        buf->putUtf8(key);
        buf->putUtf8(value);
        buf->putVar32(start, buf->offset() - start);
        flushIfNeeded(buf);
    }

    void writeBoolSetting(Buffer* buf, int category, const char* key, bool value) {
        writeStringSetting(buf, category, key, value ? "true" : "false");
    }

    void writeIntSetting(Buffer* buf, int category, const char* key, long value) {
        char str[32];
        sprintf(str, "%ld", value);
        writeStringSetting(buf, category, key, str);
    }

    void writeListSetting(Buffer* buf, int category, const char* key, const char* base, int offset) {
        while (offset != 0) {
            writeStringSetting(buf, category, key, base + offset);
            offset = ((int*)(base + offset))[-1];
        }
    }

    void writeOsCpuInfo(Buffer* buf) {
        struct utsname u;
        if (uname(&u) != 0) {
            return;
        }

        char str[512];
        snprintf(str, sizeof(str) - 1, "uname: %s %s %s %s", u.sysname, u.release, u.version, u.machine);
        str[sizeof(str) - 1] = 0;

        int start = buf->skip(5);
        buf->put8(T_OS_INFORMATION);
        buf->putVar64(_start_nanos);
        buf->putUtf8(str);
        buf->putVar32(start, buf->offset() - start);

        start = buf->skip(5);
        buf->put8(T_CPU_INFORMATION);
        buf->putVar64(_start_nanos);
        buf->putUtf8(u.machine);
        buf->putUtf8(OS::getCpuDescription(str, sizeof(str) - 1) ? str : "");
        buf->putVar32(1);
        buf->putVar32(_available_processors);
        buf->putVar32(_available_processors);
        buf->putVar32(start, buf->offset() - start);
    }

    void writeJvmInfo(Buffer* buf) {
        if (_agent_properties == NULL && !parseAgentProperties()) {
            return;
        }

        char* jvm_name = NULL;
        char* jvm_version = NULL;

        jvmtiEnv* jvmti = VM::jvmti();
        jvmti->GetSystemProperty("java.vm.name", &jvm_name);
        jvmti->GetSystemProperty("java.vm.version", &jvm_version);

        flushIfNeeded(buf, RECORDING_BUFFER_LIMIT - 5 * MAX_STRING_LENGTH);
        int start = buf->skip(5);
        buf->put8(T_JVM_INFORMATION);
        buf->putVar64(_start_nanos);
        buf->putUtf8(jvm_name);
        buf->putUtf8(jvm_version);
        buf->putUtf8(_jvm_args);
        buf->putUtf8(_jvm_flags);
        buf->putUtf8(_java_command);
        buf->putVar64(OS::processStartTime());
        buf->putVar32(OS::processId());
        buf->putVar32(start, buf->offset() - start);

        jvmti->Deallocate((unsigned char*)jvm_version);
        jvmti->Deallocate((unsigned char*)jvm_name);
    }

    void writeSystemProperties(Buffer* buf) {
        jvmtiEnv* jvmti = VM::jvmti();
        jint count;
        char** keys;
        if (jvmti->GetSystemProperties(&count, &keys) != 0) {
            return;
        }

        for (int i = 0; i < count; i++) {
            char* key = keys[i];
            char* value = NULL;
            if (jvmti->GetSystemProperty(key, &value) == 0) {
                flushIfNeeded(buf, RECORDING_BUFFER_LIMIT - 2 * MAX_STRING_LENGTH);
                int start = buf->skip(5);
                buf->put8(T_INITIAL_SYSTEM_PROPERTY);
                buf->putVar64(_start_nanos);
                buf->putUtf8(key);
                buf->putUtf8(value);
                buf->putVar32(start, buf->offset() - start);
                jvmti->Deallocate((unsigned char*)value);
            }
        }

        jvmti->Deallocate((unsigned char*)keys);
    }

    void writeNativeLibraries(Buffer* buf) {
        if (_recorded_lib_count < 0) return;

        Profiler* profiler = Profiler::instance();
        NativeCodeCache** native_libs = profiler->_native_libs;
        int native_lib_count = profiler->_native_lib_count;

        for (int i = _recorded_lib_count; i < native_lib_count; i++) {
            flushIfNeeded(buf, RECORDING_BUFFER_LIMIT - MAX_STRING_LENGTH);
            int start = buf->skip(5);
            buf->put8(T_NATIVE_LIBRARY);
            buf->putVar64(_start_nanos);
            buf->putUtf8(native_libs[i]->name());
            buf->putVar64((uintptr_t) native_libs[i]->minAddress());
            buf->putVar64((uintptr_t) native_libs[i]->maxAddress());
            buf->putVar32(start, buf->offset() - start);
        }

        _recorded_lib_count = native_lib_count;
    }

    void writeCpool(Buffer* buf) {
        buf->skip(5);  // size will be patched later
        buf->putVar32(T_CPOOL);
        buf->putVar64(_start_nanos);
        buf->putVar32(0);
        buf->putVar32(0);
        buf->putVar32(1);

        buf->putVar32(9);

        Lookup lookup;
        writeFrameTypes(buf);
        writeThreadStates(buf);
        writeThreads(buf);
        writeStackTraces(buf, &lookup);
        writeMethods(buf, &lookup);
        writeClasses(buf, &lookup);
        writePackages(buf, &lookup);
        writeSymbols(buf, &lookup);
        writeLogLevels(buf);
    }

    void writeFrameTypes(Buffer* buf) {
        buf->putVar32(T_FRAME_TYPE);
        buf->putVar32(6);
        buf->putVar32(FRAME_INTERPRETED);  buf->putUtf8("Interpreted");
        buf->putVar32(FRAME_JIT_COMPILED); buf->putUtf8("JIT compiled");
        buf->putVar32(FRAME_INLINED);      buf->putUtf8("Inlined");
        buf->putVar32(FRAME_NATIVE);       buf->putUtf8("Native");
        buf->putVar32(FRAME_CPP);          buf->putUtf8("C++");
        buf->putVar32(FRAME_KERNEL);       buf->putUtf8("Kernel");
    }

    void writeThreadStates(Buffer* buf) {
        buf->putVar32(T_THREAD_STATE);
        buf->putVar32(2);
        buf->putVar32(THREAD_RUNNING);     buf->putUtf8("STATE_RUNNABLE");
        buf->putVar32(THREAD_SLEEPING);    buf->putUtf8("STATE_SLEEPING");
    }

    void writeThreads(Buffer* buf) {
        std::vector<int> threads;
        _thread_set.collect(threads);

        Profiler* profiler = Profiler::instance();
        MutexLocker ml(profiler->_thread_names_lock);
        std::map<int, std::string>& thread_names = profiler->_thread_names;
        std::map<int, jlong>& thread_ids = profiler->_thread_ids;
        char name_buf[32];

        buf->putVar32(T_THREAD);
        buf->putVar32(threads.size());
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

            buf->putVar32(threads[i]);
            buf->putUtf8(thread_name);
            buf->putVar32(threads[i]);
            if (thread_id == 0) {
                buf->put8(0);
            } else {
                buf->putUtf8(thread_name);
            }
            buf->putVar64(thread_id);
            flushIfNeeded(buf);
        }
    }

    void writeStackTraces(Buffer* buf, Lookup* lookup) {
        std::map<u32, CallTrace*> traces;
        Profiler::instance()->_call_trace_storage.collectTraces(traces);

        buf->putVar32(T_STACK_TRACE);
        buf->putVar32(traces.size());
        for (std::map<u32, CallTrace*>::const_iterator it = traces.begin(); it != traces.end(); ++it) {
            CallTrace* trace = it->second;
            buf->putVar32(it->first);
            buf->putVar32(0);  // truncated
            buf->putVar32(trace->num_frames);
            for (int i = 0; i < trace->num_frames; i++) {
                MethodInfo* mi = lookup->resolveMethod(trace->frames[i]);
                buf->putVar32(mi->_key);
                jint bci = trace->frames[i].bci;
                if (bci >= 0) {
                    buf->putVar32(mi->getLineNumber(bci));
                    buf->putVar32(bci);
                } else {
                    buf->put8(0);
                    buf->put8(0);
                }
                buf->putVar32(mi->_type);
                flushIfNeeded(buf);
            }
            flushIfNeeded(buf);
        }
    }

    void writeMethods(Buffer* buf, Lookup* lookup) {
        std::map<jmethodID, MethodInfo>& method_map = lookup->_method_map;

        buf->putVar32(T_METHOD);
        buf->putVar32(method_map.size());
        for (std::map<jmethodID, MethodInfo>::const_iterator it = method_map.begin(); it != method_map.end(); ++it) {
            const MethodInfo& mi = it->second;
            buf->putVar32(mi._key);
            buf->putVar32(mi._class);
            buf->putVar32(mi._name);
            buf->putVar32(mi._sig);
            buf->putVar32(mi._modifiers);
            buf->putVar32(0);  // hidden
            flushIfNeeded(buf);
        }
    }

    void writeClasses(Buffer* buf, Lookup* lookup) {
        std::map<u32, const char*> classes;
        lookup->_classes->collect(classes);

        buf->putVar32(T_CLASS);
        buf->putVar32(classes.size());
        for (std::map<u32, const char*>::const_iterator it = classes.begin(); it != classes.end(); ++it) {
            const char* name = it->second;
            buf->putVar32(it->first);
            buf->putVar32(0);  // classLoader
            buf->putVar32(lookup->getSymbol(name));
            buf->putVar32(lookup->getPackage(name));
            buf->putVar32(0);  // access flags
            flushIfNeeded(buf);
        }
    }

    void writePackages(Buffer* buf, Lookup* lookup) {
        std::map<u32, const char*> packages;
        lookup->_packages.collect(packages);

        buf->putVar32(T_PACKAGE);
        buf->putVar32(packages.size());
        for (std::map<u32, const char*>::const_iterator it = packages.begin(); it != packages.end(); ++it) {
            buf->putVar32(it->first);
            buf->putVar32(lookup->getSymbol(it->second));
            flushIfNeeded(buf);
        }
    }

    void writeSymbols(Buffer* buf, Lookup* lookup) {
        std::map<u32, const char*> symbols;
        lookup->_symbols.collect(symbols);

        buf->putVar32(T_SYMBOL);
        buf->putVar32(symbols.size());
        for (std::map<u32, const char*>::const_iterator it = symbols.begin(); it != symbols.end(); ++it) {
            buf->putVar32(it->first);
            buf->putUtf8(it->second);
            flushIfNeeded(buf);
        }
    }

    void writeLogLevels(Buffer* buf) {
        buf->putVar32(T_LOG_LEVEL);
        buf->putVar32(LOG_ERROR - LOG_TRACE + 1);
        for (int i = LOG_TRACE; i <= LOG_ERROR; i++) {
            buf->putVar32(i);
            buf->putUtf8(Log::LEVEL_NAME[i]);
        }
    }

    void recordExecutionSample(Buffer* buf, int tid, u32 call_trace_id, ExecutionEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_EXECUTION_SAMPLE);
        buf->putVar64(OS::nanotime());
        buf->putVar32(tid);
        buf->putVar32(call_trace_id);
        buf->putVar32(event->_thread_state);
        buf->put8(start, buf->offset() - start);
    }

    void recordAllocationInNewTLAB(Buffer* buf, int tid, u32 call_trace_id, AllocEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_ALLOC_IN_NEW_TLAB);
        buf->putVar64(OS::nanotime());
        buf->putVar32(tid);
        buf->putVar32(call_trace_id);
        buf->putVar32(event->_class_id);
        buf->putVar64(event->_instance_size);
        buf->putVar64(event->_total_size);
        buf->put8(start, buf->offset() - start);
    }

    void recordAllocationOutsideTLAB(Buffer* buf, int tid, u32 call_trace_id, AllocEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_ALLOC_OUTSIDE_TLAB);
        buf->putVar64(OS::nanotime());
        buf->putVar32(tid);
        buf->putVar32(call_trace_id);
        buf->putVar32(event->_class_id);
        buf->putVar64(event->_total_size);
        buf->put8(start, buf->offset() - start);
    }

    void recordMonitorBlocked(Buffer* buf, int tid, u32 call_trace_id, LockEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_MONITOR_ENTER);
        buf->putVar64(event->_start_time);
        buf->putVar64(event->_end_time - event->_start_time);
        buf->putVar32(tid);
        buf->putVar32(call_trace_id);
        buf->putVar32(event->_class_id);
        buf->putVar64(event->_address);
        buf->put8(start, buf->offset() - start);
    }

    void recordThreadPark(Buffer* buf, int tid, u32 call_trace_id, LockEvent* event) {
        int start = buf->skip(1);
        buf->put8(T_THREAD_PARK);
        buf->putVar64(event->_start_time);
        buf->putVar64(event->_end_time - event->_start_time);
        buf->putVar32(tid);
        buf->putVar32(call_trace_id);
        buf->putVar32(event->_class_id);
        buf->putVar64(event->_timeout);
        buf->putVar64(event->_address);
        buf->put8(start, buf->offset() - start);
    }

    void recordCpuLoad(Buffer* buf, float proc_user, float proc_system, float machine_total) {
        int start = buf->skip(1);
        buf->put8(T_CPU_LOAD);
        buf->putVar64(OS::nanotime());
        buf->putFloat(proc_user);
        buf->putFloat(proc_system);
        buf->putFloat(machine_total);
        buf->put8(start, buf->offset() - start);
    }

    void addThread(int tid) {
        if (!_thread_set.accept(tid)) {
            _thread_set.add(tid);
        }
    }
};

SpinLock Recording::_cpu_monitor_lock(1);
int Recording::_append_fd = -1;

char* Recording::_agent_properties = NULL;
char* Recording::_jvm_args = NULL;
char* Recording::_jvm_flags = NULL;
char* Recording::_java_command = NULL;


Error FlightRecorder::start(Arguments& args, bool reset) {
    if (args._file == NULL || args._file[0] == 0) {
        return Error("Flight Recorder output file is not specified");
    }

    if (args.hasOption(JFR_SYNC) && !loadJavaHelper()) {
        return Error("Could not load JFR combiner class");
    }

    int fd = open(args._file, O_CREAT | O_RDWR | (reset ? O_TRUNC : 0), 0644);
    if (fd == -1) {
        return Error("Could not open Flight Recorder output file");
    }

    if (args.hasOption(JFR_TEMP_FILE)) {
        unlink(args._file);
    }

    _rec = new Recording(fd, args);
    _rec_lock.unlock();
    return Error::OK;
}

void FlightRecorder::stop() {
    if (_rec != NULL) {
        _rec_lock.lock();
        delete _rec;
        _rec = NULL;
    }
}

void FlightRecorder::flush() {
    if (_rec != NULL) {
        _rec_lock.lock();
        _rec->switchChunk();
        _rec_lock.unlock();
    }
}

bool FlightRecorder::loadJavaHelper() {
    if (!_java_helper_loaded) {
        JNIEnv* jni = VM::jni();
        const JNINativeMethod native_method = {
            (char*)"appendRecording", (char*)"(Ljava/lang/String;)V",
            (void*)Recording::appendRecording
        };

        jclass cls = jni->DefineClass(NULL, NULL, (const jbyte*)JFR_COMBINER_CLASS, sizeof(JFR_COMBINER_CLASS));
        if (cls == NULL || jni->RegisterNatives(cls, &native_method, 1) != 0 || jni->GetMethodID(cls, "<init>", "()V") == NULL) {
            jni->ExceptionClear();
            return false;
        }

        _java_helper_loaded = true;
    }

    return true;
}

void FlightRecorder::recordEvent(int lock_index, int tid, u32 call_trace_id,
                                 int event_type, Event* event, u64 counter) {
    if (_rec != NULL) {
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

void FlightRecorder::recordLog(LogLevel level, const char* message, size_t len) {
    if (!_rec_lock.tryLockShared()) {
        // No active recording
        return;
    }

    if (len > MAX_STRING_LENGTH) len = MAX_STRING_LENGTH;
    Buffer* buf = (Buffer*)alloca(len + 40);
    buf->reset();

    int start = buf->skip(5);
    buf->put8(T_LOG);
    buf->putVar64(OS::nanotime());
    buf->put8(level);
    buf->putUtf8(message, len);
    buf->putVar32(start, buf->offset() - start);
    _rec->flush(buf);

    _rec_lock.unlockShared();
}
