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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#include "asyncProfiler.h"

static JavaVM* jvm;
static jvmtiEnv* jvmti;
static u64 calltrace_hash[MAX_CALLTRACES];
static CallTraceSnapshot calltrace_snapshot[MAX_CALLTRACES];
static int calls_total, calls_in_java, calls_with_traces;

static void loadMethodIDs(jvmtiEnv* jvmti, jclass klass) {
    jint method_count;
    jmethodID* methods;
    if (jvmti->GetClassMethods(klass, &method_count, &methods) == 0) {
        jvmti->Deallocate((unsigned char*)methods);
    }
}

static void loadAllMethodIDs(jvmtiEnv* jvmti) {
    jint class_count;
    jclass* classes;
    if (jvmti->GetLoadedClasses(&class_count, &classes) == 0) {
        for (int i = 0; i < class_count; i++) {
            loadMethodIDs(jvmti, classes[i]);
        }
        jvmti->Deallocate((unsigned char*)classes);
    }
}

static void JNICALL VMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    loadAllMethodIDs(jvmti);
}

static void JNICALL ClassLoad(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
    // Needed only for AsyncGetCallTrace support
}

static void JNICALL ClassPrepare(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
    loadMethodIDs(jvmti, klass);
}

static void initJvmti(JavaVM* vm) {
    jvm = vm;
    vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_0);

    jvmtiCapabilities capabilities = {0};
    capabilities.can_generate_all_class_hook_events = 1;
    capabilities.can_get_bytecodes = 1;
    capabilities.can_get_constant_pool = 1;
    capabilities.can_get_source_file_name = 1;
    capabilities.can_get_line_numbers = 1;
    jvmti->AddCapabilities(&capabilities);

    jvmtiEventCallbacks callbacks = {0};
    callbacks.VMInit = VMInit;
    callbacks.ClassLoad = ClassLoad;
    callbacks.ClassPrepare = ClassPrepare;
    jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));

    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_LOAD, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, NULL);
}

static u64 hashCode(ASGCT_CallTrace* trace) {
    const u64 M = 0xc6a4a7935bd1e995LL;
    const int R = 47;

    u64 h = trace->num_frames * M;

    for (int i = 0; i < trace->num_frames; i++) {
        u64 k = ((u64)trace->frames[i].bci << 32) ^ (u64)trace->frames[i].method_id;
        k *= M;
        k ^= k >> R;
        k *= M;
        h ^= k;
        h *= M;
    }

    h ^= h >> R;
    h *= M;
    h ^= h >> R;

    return h;
}

static void storeCallTrace(ASGCT_CallTrace* trace) {
    u64 hash = hashCode(trace);
    int bucket = (int)(hash % MAX_CALLTRACES);
    int i = bucket;

    do {
        if (calltrace_hash[i] == hash && calltrace_snapshot[i].call_count > 0) {
            calltrace_snapshot[i].call_count++;
            return;
        } else if (calltrace_hash[i] == 0) {
            break;
        }
        if (++i == MAX_CALLTRACES) i = 0;
    } while (i != bucket);

    calltrace_hash[i] = hash;

    CallTraceSnapshot* snapshot = &calltrace_snapshot[i];
    snapshot->call_count = 1;
    snapshot->num_frames = trace->num_frames;
    for (int j = 0; j < trace->num_frames; j++) {
        snapshot->frames[j] = trace->frames[j];
    }
}

static int snapshotComparator(const void* s1, const void* s2) {
    return ((CallTraceSnapshot*)s2)->call_count - ((CallTraceSnapshot*)s1)->call_count;
}

static char* format_class_name(char* class_name) {
    for (char* s = class_name; *s != 0; s++) {
        if (*s == '/') *s = '.';
    }
    class_name[strlen(class_name) - 1] = 0;
    return class_name + 1;
}

static void dumpCallTrace(CallTraceSnapshot* trace) {
    for (int i = 0; i < trace->num_frames; i++) {
        ASGCT_CallFrame* frame = &trace->frames[i];
        if (frame->method_id != NULL) {
            char* name;
            char* sig;
            char* class_sig;
            jclass method_class;
            jvmti->GetMethodName(frame->method_id, &name, &sig, NULL);
            jvmti->GetMethodDeclaringClass(frame->method_id, &method_class);
            jvmti->GetClassSignature(method_class, &class_sig, NULL);
            printf("  [%2d] %s.%s @%d\n", i, format_class_name(class_sig), name, frame->bci);
            jvmti->Deallocate((unsigned char*)name);
            jvmti->Deallocate((unsigned char*)sig);
            jvmti->Deallocate((unsigned char*)class_sig);
        }
    }
}

static void dumpCallTraces() {
    printf("total = %d, in_java = %d, with_traces = %d\n", calls_total, calls_in_java, calls_with_traces);
    
    qsort(calltrace_snapshot, MAX_CALLTRACES, sizeof(CallTraceSnapshot), snapshotComparator);

    for (int i = 0; i < MAX_CALLTRACES; i++) {
        CallTraceSnapshot* trace = &calltrace_snapshot[i];
        if (trace->call_count > 0) {
            printf("\nSamples: %d (%.2f%%)\n", trace->call_count, 100.0f * trace->call_count / calls_total);
            dumpCallTrace(trace);
        }
    }
}

static void sigprofHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    calls_total++;

    JNIEnv* env;
    if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != 0) {
        return;
    }

    calls_in_java++;

    ASGCT_CallFrame frames[MAX_FRAMES];
    ASGCT_CallTrace trace = {env, sizeof(frames) / sizeof(frames[0]), frames};
    AsyncGetCallTrace(&trace, trace.num_frames, ucontext);

    if (trace.num_frames > 0) {
        calls_with_traces++;
        storeCallTrace(&trace);
    }
}

static void setProfilingTimer(long sec, long usec) {
    struct sigaction sa;
    sa.sa_handler = (sec | usec) == 0 ? SIG_IGN : NULL;
    sa.sa_sigaction = (sec | usec) == 0 ? NULL : sigprofHandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, NULL);

    struct itimerval itv = {{sec, usec}, {sec, usec}};
    setitimer(ITIMER_PROF, &itv, NULL);
}

static void initProfiler(char* options) {
    if (strcmp(options, "start") == 0) {
        printf("Profiling started\n");
        memset(calltrace_hash, 0, sizeof(calltrace_hash));
        memset(calltrace_snapshot, 0, sizeof(calltrace_snapshot));
        calls_total = calls_in_java = calls_with_traces = 0;
        setProfilingTimer(0, 10000);
    } else if (strcmp(options, "stop") == 0) {
        printf("Profiling stopped\n");
        setProfilingTimer(0, 0);
        dumpCallTraces();
    }
}

extern "C" JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    initJvmti(vm);
    initProfiler("start");
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    initJvmti(vm);
    loadAllMethodIDs(jvmti);
    initProfiler(options);
    return 0;
}
