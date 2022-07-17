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
#include "incbin.h"
#include "javaApi.h"
#include "os.h"
#include "profiler.h"
#include "vmStructs.h"


INCBIN(SERVER_CLASS, "one/profiler/Server.class")


static void throwNew(JNIEnv* env, const char* exception_class, const char* message) {
    jclass cls = env->FindClass(exception_class);
    if (cls != NULL) {
        env->ThrowNew(cls, message);
    }
}
extern "C" JNIEXPORT jlong JNICALL
Java_one_profiler_AsyncProfiler_getMethodID(JNIEnv* env, jclass unused, jclass klass, jstring method, jstring sig) {
  const char* method_str = env->GetStringUTFChars(method, NULL);
  const char* sig_str = env->GetStringUTFChars(sig, NULL);
  jmethodID id = env->GetMethodID(klass, method_str, sig_str);
  env->ReleaseStringUTFChars(method, method_str);
  env->ReleaseStringUTFChars(sig, sig_str);
  return (jlong) id;
}

extern "C" JNIEXPORT jlong JNICALL
Java_one_profiler_AsyncProfiler_getStaticMethodID(JNIEnv* env, jobject unused, jclass klass, jstring method, jstring sig) {
  const char* method_str = env->GetStringUTFChars(method, NULL);
  const char* sig_str = env->GetStringUTFChars(sig, NULL);
  jmethodID id = env->GetStaticMethodID(klass, method_str, sig_str);

  env->ReleaseStringUTFChars(sig, sig_str);
  return (jlong) id;
}
extern "C" JNIEXPORT long JNICALL
Java_one_profiler_AsyncProfiler_setAwaitStackId(JNIEnv* env, jobject unused, jlong id, jlong signal, jlong insertionId) {
  return Profiler::instance()->setAwaitStackId((long)id, (long) signal, (jmethodID) insertionId);
}

extern "C" JNIEXPORT long JNICALL
Java_one_profiler_AsyncProfiler_getAwaitSampledSignal(JNIEnv* env, jobject unused, jlong id, jlong setWhenSampled) {
  return Profiler::instance()->getAwaitSampledSignal();
}

extern "C" JNIEXPORT long JNICALL
Java_one_profiler_AsyncProfiler_saveAwaitFrames(JNIEnv* env, jclass unused, int ft, jlongArray ids, jint nids) {
  jlong *elems = (jlong*) env->GetPrimitiveArrayCritical(ids, 0);
  long ret = Profiler::instance()->saveAwaitFrames(static_cast<AwaitFrameType>(ft), elems, nids);
  env->ReleasePrimitiveArrayCritical(ids, (void*) elems, 0);
  return ret;
}

extern "C" JNIEXPORT long JNICALL
Java_one_profiler_AsyncProfiler_saveString(JNIEnv* env, jclass unused, jstring name) {
    const char* name_str = env->GetStringUTFChars(name, NULL);
    int l = strlen(name_str);
    char *p = new char[l+5];
    snprintf(p, l+5, "%s_[a]",name_str);
    env->ReleaseStringUTFChars(name, name_str);
    return (long) p;
}

extern "C" JNIEXPORT jint JNICALL
Java_one_profiler_AsyncProfiler_getThread(JNIEnv* env, jobject unused) {
  return (jint) OS::threadId();
}


extern "C" DLLEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_start0(JNIEnv* env, jobject unused, jstring event, jlong interval, jboolean reset) {
    Arguments args;
    const char* event_str = env->GetStringUTFChars(event, NULL);
    if (strcmp(event_str, EVENT_ALLOC) == 0) {
        args._alloc = interval > 0 ? interval : 0;
    } else if (strcmp(event_str, EVENT_LOCK) == 0) {
        args._lock = interval > 0 ? interval : 0;
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
        std::ostringstream out;
        error = Profiler::instance()->runInternal(args, out);
        if (!error) {
            if (out.tellp() >= 0x3fffffff) {
                throwNew(env, "java/lang/IllegalStateException", "Output exceeds string size limit");
                return NULL;
            }
            return env->NewStringUTF(out.str().c_str());
        }
    } else {
        std::ofstream out(args.file(), std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            throwNew(env, "java/io/IOException", strerror(errno));
            return NULL;
        }
        error = Profiler::instance()->runInternal(args, out);
        out.close();
        if (!error) {
            return env->NewStringUTF("OK");
        }
    }

    throwNew(env, "java/lang/IllegalStateException", error.message());
    return NULL;
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


#define F(name, sig)  {(char*)#name, (char*)sig, (void*)Java_one_profiler_AsyncProfiler_##name}

static const JNINativeMethod profiler_natives[] = {
    F(start0,        "(Ljava/lang/String;JZ)V"),
    F(stop0,         "()V"),
    F(execute0,      "(Ljava/lang/String;)Ljava/lang/String;"),
    F(getSamples,    "()J"),
    F(filterThread0, "(Ljava/lang/Thread;Z)V"),
    F(getMethodID,   "(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/String;)J"),
    F(getStaticMethodID,   "(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/String;)J"),
    F(setAwaitStackId, "(JJJ)J"),
    F(getAwaitSampledSignal, "()J"),
    F(saveAwaitFrames, "(I[JI)J"),
    F(saveString,    "(Ljava/lang/String;)J"),
    F(getThread,"()I")
};

static const JNINativeMethod* execute0 = &profiler_natives[2];

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
        jclass cls = jni->DefineClass(NULL, loader, (const jbyte*)SERVER_CLASS, INCBIN_SIZEOF(SERVER_CLASS));
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
