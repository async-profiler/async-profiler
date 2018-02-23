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

#include <sstream>
#include "arguments.h"
#include "profiler.h"


static void throw_new(JNIEnv* env, const char* exception_class, const char* message) {
    jclass cls = env->FindClass(exception_class);
    if (cls != NULL) {
        env->ThrowNew(cls, message);
    }
}


extern "C" JNIEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_start0(JNIEnv* env, jobject unused, jstring event, jlong interval) {
    const char* event_str = env->GetStringUTFChars(event, NULL);
    Error error = Profiler::_instance.start(event_str, interval, DEFAULT_FRAMEBUF, false);
    env->ReleaseStringUTFChars(event, event_str);

    if (error) {
        throw_new(env, "java/lang/IllegalStateException", error.message());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_stop0(JNIEnv* env, jobject unused) {
    Error error = Profiler::_instance.stop();

    if (error) {
        throw_new(env, "java/lang/IllegalStateException", error.message());
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_one_profiler_AsyncProfiler_getSamples(JNIEnv* env, jobject unused) {
    return (jlong)Profiler::_instance.total_samples();
}

extern "C" JNIEXPORT jstring JNICALL
Java_one_profiler_AsyncProfiler_dump0(JNIEnv* env, jobject unused, jstring options) {
    Arguments args;
    const char* options_str = env->GetStringUTFChars(options, NULL);
    Error error = args.parse(options_str);
    env->ReleaseStringUTFChars(options, options_str);

    if (error) {
        throw_new(env, "java/lang/IllegalArgumentException", error.message());
        return NULL;
    } else if (args._action != ACTION_DUMP) {
        throw_new(env, "java/lang/IllegalArgumentException", "Unrecognized output format");
        return NULL;
    }

    std::ostringstream out;
    Profiler::_instance.runInternal(args, out);
    return env->NewStringUTF(out.str().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_one_profiler_AsyncProfiler_dumpCollapsed0(JNIEnv* env, jobject unused, jint counter) {
    Arguments args;
    args._counter = counter == COUNTER_SAMPLES ? COUNTER_SAMPLES : COUNTER_TOTAL;

    std::ostringstream out;
    Profiler::_instance.dumpCollapsed(out, args);
    return env->NewStringUTF(out.str().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_one_profiler_AsyncProfiler_dumpTraces0(JNIEnv* env, jobject unused, jint max_traces) {
    std::ostringstream out;
    Profiler::_instance.dumpSummary(out);
    Profiler::_instance.dumpTraces(out, max_traces ? max_traces : MAX_CALLTRACES);
    return env->NewStringUTF(out.str().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_one_profiler_AsyncProfiler_dumpFlat0(JNIEnv* env, jobject unused, jint max_methods) {
    std::ostringstream out;
    Profiler::_instance.dumpSummary(out);
    Profiler::_instance.dumpFlat(out, max_methods ? max_methods : MAX_CALLTRACES);
    return env->NewStringUTF(out.str().c_str());
}
