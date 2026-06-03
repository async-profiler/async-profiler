/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>
#include "asprof.h"
#include "incbin.h"
#include "javaApi.h"
#include "os.h"
#include "profiler.h"
#include "threadLocalData.h"
#include "tsc.h"
#include "vmStructs.h"


INCLUDE_HELPER_CLASS(SERVER_NAME, SERVER_CLASS, "one/profiler/Server")


static void throwNew(JNIEnv* env, const char* exception_class, const char* message) {
    jclass cls = env->FindClass(exception_class);
    if (cls != NULL) {
        env->ThrowNew(cls, message);
    }
}


extern "C" DLLEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_start0(JNIEnv* env, jobject unused, jstring event, jlong interval, jboolean reset) {
    Arguments args;
    const char* event_str = env->GetStringUTFChars(event, NULL);
    if (strcmp(event_str, EVENT_ALLOC) == 0) {
        args._alloc = interval > 0 ? interval : 0;
    } else if (strcmp(event_str, EVENT_LOCK) == 0) {
        args._lock = interval >= 0 ? interval : DEFAULT_LOCK_INTERVAL;
    } else {
        args._event = event_str;
        args._interval = interval;
    }

    Error error = Profiler::instance()->start(args, reset);
    env->ReleaseStringUTFChars(event, event_str);

    if (error) {
        throwNew(env, "java/lang/IllegalStateException", error.message());
    }
}

extern "C" DLLEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_stop0(JNIEnv* env, jobject unused) {
    Error error = Profiler::instance()->stop();

    if (error) {
        throwNew(env, "java/lang/IllegalStateException", error.message());
    }
}

extern "C" DLLEXPORT jstring JNICALL
Java_one_profiler_AsyncProfiler_execute0(JNIEnv* env, jobject unused, jstring command) {
    Arguments args;
    const char* command_str = env->GetStringUTFChars(command, NULL);
    Error error = args.parse(command_str);
    env->ReleaseStringUTFChars(command, command_str);

    if (error) {
        throwNew(env, "java/lang/IllegalArgumentException", error.message());
        return NULL;
    }

    Log::open(args);

    if (!args.hasOutputFile()) {
        BufferWriter out;
        error = Profiler::instance()->runInternal(args, out);
        if (!error) {
            out << '\0';
            if (out.size() >= 0x3fffffff) {
                throwNew(env, "java/lang/IllegalStateException", "Output exceeds string size limit");
                return NULL;
            }
            return env->NewStringUTF(out.buf());
        }
    } else {
        FileWriter out(args.file());
        if (!out.is_open()) {
            throwNew(env, "java/io/IOException", strerror(errno));
            return NULL;
        }
        error = Profiler::instance()->runInternal(args, out);
        if (!error) {
            return env->NewStringUTF("OK");
        }
    }

    throwNew(env, "java/lang/IllegalStateException", error.message());
    return NULL;
}

extern "C" DLLEXPORT jbyteArray JNICALL
Java_one_profiler_AsyncProfiler_execute1(JNIEnv* env, jobject unused, jstring command) {
    Arguments args;
    const char* command_str = env->GetStringUTFChars(command, NULL);
    Error error = args.parse(command_str);
    env->ReleaseStringUTFChars(command, command_str);

    if (error) {
        throwNew(env, "java/lang/IllegalArgumentException", error.message());
        return NULL;
    }
    if (args.hasOutputFile()) {
        throwNew(env, "java/lang/IllegalArgumentException", "execute1 calls should not specify an output file argument");
        return NULL;
    }

    Log::open(args);

    BufferWriter out;
    // TODO: This is doing one more copy than necessary, from ProtoWriter to BufferWriter
    error = Profiler::instance()->runInternal(args, out);
    if (error) {
        throwNew(env, "java/lang/IllegalStateException", error.message());
        return NULL;
    }

    jbyteArray output = env->NewByteArray(out.size());
    env->SetByteArrayRegion(output, 0, out.size(), (const jbyte*) out.buf());
    return output;
}

extern "C" DLLEXPORT jlong JNICALL
Java_one_profiler_AsyncProfiler_getSamples(JNIEnv* env, jobject unused) {
    return (jlong)Profiler::instance()->total_samples();
}

extern "C" DLLEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_filterThread0(JNIEnv* env, jobject unused, jthread thread, jboolean enable) {
    int thread_id;
    if (thread == NULL) {
        thread_id = OS::threadId();
    } else if ((thread_id = VMThread::nativeThreadId(env, thread)) < 0) {
        return;
    }

    ThreadFilter* thread_filter = Profiler::instance()->threadFilter();
    if (enable) {
        thread_filter->add(thread_id);
    } else {
        thread_filter->remove(thread_id);
    }
}

extern "C" DLLEXPORT jint JNICALL
Java_one_profiler_Recording_registerNatives(JNIEnv* env, jclass cls) {
    return RecordingAPI::registerNatives(env, cls);
}

extern "C" DLLEXPORT jobject JNICALL
Java_one_profiler_Recording_getThreadLocalBuffer(JNIEnv* env, jclass cls) {
    asprof_thread_local_data* tld = ThreadLocalData::get();
    return tld == nullptr ? nullptr : env->NewDirectByteBuffer(tld, sizeof(*tld));
}

extern "C" DLLEXPORT void JNICALL
Java_one_profiler_Recording_emitSpan(JNIEnv* env, jclass cls, jlong startTime, jlong endTime, jstring tag) {
    if ((u64)startTime > (u64)endTime) return;

    SpanEvent event;
    event._start_time = startTime;
    event._end_time = endTime;
    event._tag = tag != nullptr ? env->GetStringUTFChars(tag, nullptr) : nullptr;

    Profiler::instance()->recordEventOnly(SPAN, &event);

    if (tag != nullptr) {
        env->ReleaseStringUTFChars(tag, event._tag);
    }
}


#define F(cls, name, sig)  {(char*)#name, (char*)sig, (void*)Java_one_profiler_##cls##_##name}

static const JNINativeMethod profiler_natives[] = {
    F(AsyncProfiler, start0,        "(Ljava/lang/String;JZ)V"),
    F(AsyncProfiler, stop0,         "()V"),
    F(AsyncProfiler, execute0,      "(Ljava/lang/String;)Ljava/lang/String;"),
    F(AsyncProfiler, execute1,      "(Ljava/lang/String;)[B"),
    F(AsyncProfiler, getSamples,    "()J"),
    F(AsyncProfiler, filterThread0, "(Ljava/lang/Thread;Z)V"),
};

static const JNINativeMethod* execute0 = &profiler_natives[2];

static const JNINativeMethod recording_natives[] = {
    F(Recording, getThreadLocalBuffer, "()Ljava/nio/ByteBuffer;"),
    F(Recording, emitSpan,             "(JJLjava/lang/String;)V"),
};

#undef F


// Since AsyncProfiler class can be renamed or moved to another package (shaded),
// we look for the actual class in the stack trace.
void JavaAPI::registerNatives(jvmtiEnv* jvmti, JNIEnv* jni) {
    jvmtiFrameInfo frame[10];
    jint frame_count;
    if (jvmti->GetStackTrace(NULL, 0, sizeof(frame) / sizeof(frame[0]), frame, &frame_count) != 0) {
        return;
    }

    jclass System = jni->FindClass("java/lang/System");
    jmethodID load = jni->GetStaticMethodID(System, "load", "(Ljava/lang/String;)V");
    jmethodID loadLibrary = jni->GetStaticMethodID(System, "loadLibrary", "(Ljava/lang/String;)V");

    // Look for System.load() or System.loadLibrary() method in the stack trace.
    // The next frame will belong to AsyncProfiler class.
    for (int i = 0; i < frame_count - 1; i++) {
        if (frame[i].method == load || frame[i].method == loadLibrary) {
            jclass profiler_class;
            if (jvmti->GetMethodDeclaringClass(frame[i + 1].method, &profiler_class) == 0) {
                for (int j = 0; j < sizeof(profiler_natives) / sizeof(JNINativeMethod); j++) {
                    jni->RegisterNatives(profiler_class, &profiler_natives[j], 1);
                }
            }
            break;
        }
    }

    jni->ExceptionClear();
}

bool JavaAPI::startHttpServer(jvmtiEnv* jvmti, JNIEnv* jni, const char* address) {
    jclass handler = jni->FindClass("com/sun/net/httpserver/HttpHandler");
    jobject loader;
    if (handler != NULL && jvmti->GetClassLoader(handler, &loader) == 0) {
        jclass cls = jni->DefineClass(SERVER_NAME, loader, (const jbyte*)SERVER_CLASS, INCBIN_SIZEOF(SERVER_CLASS));
        if (cls != NULL && jni->RegisterNatives(cls, execute0, 1) == 0) {
            jmethodID method = jni->GetStaticMethodID(cls, "start", "(Ljava/lang/String;)V");
            if (method != NULL) {
                jni->CallStaticVoidMethod(cls, method, jni->NewStringUTF(address));
                if (!jni->ExceptionCheck()) {
                    return true;
                }
            }
        }
    }

    jni->ExceptionDescribe();
    return false;
}


jclass RecordingAPI::_recording_class = nullptr;
jfieldID RecordingAPI::_state_field;
jmethodID RecordingAPI::_update_clock_method;
bool RecordingAPI::_tsc_enabled = false;

void RecordingAPI::updateClock(JNIEnv* env) {
    if (_tsc_enabled == TSC::enabled()) return;
    _tsc_enabled = TSC::enabled();

    do {
        jobject lookup = nullptr;
        if (_tsc_enabled) {
            jclass lookup_class = env->FindClass("java/lang/invoke/MethodHandles$Lookup");
            if (lookup_class == nullptr) break;

            jfieldID impl_field = env->GetStaticFieldID(lookup_class, "IMPL_LOOKUP", "java/lang/invoke/MethodHandles$Lookup");
            if (impl_field == nullptr) break;

            lookup = env->GetStaticObjectField(lookup_class, impl_field);
        }
        env->CallStaticVoidMethod(_recording_class, _update_clock_method, lookup);
    } while (false);

    env->ExceptionClear();
}

RecordingAPI::State RecordingAPI::registerNatives(JNIEnv* env, jclass recording_class) {
    if (env->RegisterNatives(recording_class, recording_natives, sizeof(recording_natives) / sizeof(recording_natives[0])) != 0) {
        return UNAVAILABLE;
    }

    _state_field = env->GetStaticFieldID(recording_class, "state", "I");
    if (_state_field == nullptr) {
        return UNAVAILABLE;
    }

    _update_clock_method = env->GetStaticMethodID(recording_class, "updateClock", "(Ljava/lang/invoke/MethodHandles$Lookup;)V");
    if (_update_clock_method == nullptr) {
        return UNAVAILABLE;
    }

    _recording_class = (jclass) env->NewGlobalRef(recording_class);
    updateClock(env);

    return Profiler::instance()->jfr()->active() ? RUNNING : STOPPED;
}

void RecordingAPI::bind(jvmtiEnv* jvmti, JNIEnv* env) {
    jclass recording_class = env->FindClass("one/profiler/Recording");
    if (recording_class == nullptr) {
        env->ExceptionClear();
        return;
    }

    jint status;
    if (jvmti->GetClassStatus(recording_class, &status) == 0 && (status & JVMTI_CLASS_STATUS_INITIALIZED)) {
        if (registerNatives(env, recording_class) == UNAVAILABLE) {
            env->ExceptionClear();
        }
    }
}

void RecordingAPI::start() {
    if (_recording_class == nullptr) return;

    JNIEnv* env = VM::jni();
    updateClock(env);
    env->SetStaticIntField(_recording_class, _state_field, RUNNING);
}

void RecordingAPI::stop() {
    if (_recording_class == nullptr) return;

    JNIEnv* env = VM::jni();
    env->SetStaticIntField(_recording_class, _state_field, STOPPED);
}
