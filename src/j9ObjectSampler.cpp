/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "j9ObjectSampler.h"
#include "j9Ext.h"
#include "vmEntry.h"


void J9ObjectSampler::JavaObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                      jobject object, jclass object_klass, jlong size) {
    if (_enabled && updateCounter(_allocated_bytes, size, _interval)) {
        recordAllocation(jvmti, jni, ALLOC_SAMPLE, object, object_klass, size);
    }
}

void J9ObjectSampler::VMObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                    jobject object, jclass object_klass, jlong size) {
    if (_enabled && updateCounter(_allocated_bytes, size, _interval)) {
        recordAllocation(jvmti, jni, ALLOC_OUTSIDE_TLAB, object, object_klass, size);
    }
}

Error J9ObjectSampler::check(Arguments& args) {
    if (J9Ext::InstrumentableObjectAlloc_id < 0) {
        return Error("InstrumentableObjectAlloc is not supported on this JVM");
    }
    return Error::OK;
}

Error J9ObjectSampler::start(Arguments& args) {
    Error error = check(args);
    if (error) {
        return error;
    }

    _interval = args._alloc > 0 ? args._alloc : DEFAULT_ALLOC_INTERVAL;
    _allocated_bytes = 0;

    initLiveRefs(args._live);

    jvmtiEnv* jvmti = VM::jvmti();
    if (jvmti->SetExtensionEventCallback(J9Ext::InstrumentableObjectAlloc_id, (jvmtiExtensionEvent)JavaObjectAlloc) != 0) {
        return Error("Could not enable InstrumentableObjectAlloc callback");
    }
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_START, NULL);

    return Error::OK;
}

void J9ObjectSampler::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_GARBAGE_COLLECTION_START, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, NULL);
    jvmti->SetExtensionEventCallback(J9Ext::InstrumentableObjectAlloc_id, NULL);

    dumpLiveRefs();
}
