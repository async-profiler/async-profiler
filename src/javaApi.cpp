/*
 * Copyright 2016 Andrei Pangin
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

#include <fstream>
#include <sstream>
#include <errno.h>
#include <string.h>
#include "javaApi.h"
#include "arguments.h"
#include "os.h"
#include "profiler.h"
#include "vmStructs.h"


extern "C" JNIEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_start0(JNIEnv* env, jobject unused, jstring event, jlong interval, jboolean reset) {
    Arguments args;
    const char* event_str = env->GetStringUTFChars(event, NULL);
    args.addEvent(event_str); 
    args._interval = interval;
    Error error = Profiler::_instance.start(args, reset);
    env->ReleaseStringUTFChars(event, event_str);

    if (error) {
        JavaAPI::throwNew(env, "java/lang/IllegalStateException", error.message());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_stop0(JNIEnv* env, jobject unused) {
    Error error = Profiler::_instance.stop();

    if (error) {
        JavaAPI::throwNew(env, "java/lang/IllegalStateException", error.message());
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_one_profiler_AsyncProfiler_execute0(JNIEnv* env, jobject unused, jstring command) {
    Arguments args;
    const char* command_str = env->GetStringUTFChars(command, NULL);
    Error error = args.parse(command_str);
    env->ReleaseStringUTFChars(command, command_str);

    if (error) {
        JavaAPI::throwNew(env, "java/lang/IllegalArgumentException", error.message());
        return NULL;
    }

    if (args._file == NULL || args._output == OUTPUT_JFR) {
        std::ostringstream out;
        Profiler::_instance.runInternal(args, out);
        return env->NewStringUTF(out.str().c_str());
    } else {
        std::ofstream out(args._file, std::ios::out | std::ios::trunc);
        if (out.is_open()) {
            Profiler::_instance.runInternal(args, out);
            out.close();
            return env->NewStringUTF("OK");
        } else {
            JavaAPI::throwNew(env, "java/io/IOException", strerror(errno));
            return NULL;
        }
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_one_profiler_AsyncProfiler_getSamples(JNIEnv* env, jobject unused) {
    return (jlong)Profiler::_instance.total_samples();
}

extern "C" JNIEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_filterThread0(JNIEnv* env, jobject unused, jthread thread, jboolean enable) {
    int thread_id;
    if (thread == NULL) {
        thread_id = OS::threadId();
    } else if (VMThread::hasNativeId()) {
        VMThread* vmThread = VMThread::fromJavaThread(env, thread);
        if (vmThread == NULL) {
            return;
        }
        thread_id = vmThread->osThreadId();
    } else {
        return;
    }

    ThreadFilter* thread_filter = Profiler::_instance.threadFilter();
    if (enable) {
        thread_filter->add(thread_id);
    } else {
        thread_filter->remove(thread_id);
    }
}


#define F(name, sig)  {(char*)#name, (char*)sig, (void*)Java_one_profiler_AsyncProfiler_##name}

static const JNINativeMethod profiler_natives[] = {
    F(start0,        "(Ljava/lang/String;JZ)V"),
    F(stop0,         "()V"),
    F(execute0,      "(Ljava/lang/String;)Ljava/lang/String;"),
    F(getSamples,    "()J"),
    F(filterThread0, "(Ljava/lang/Thread;Z)V"),
};

#undef F


void JavaAPI::throwNew(JNIEnv* env, const char* exception_class, const char* message) {
    jclass cls = env->FindClass(exception_class);
    if (cls != NULL) {
        env->ThrowNew(cls, message);
    }
}

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
                jni->RegisterNatives(profiler_class, profiler_natives, sizeof(profiler_natives) / sizeof(JNINativeMethod));
            }
            break;
        }
    }

    jni->ExceptionClear();
}
