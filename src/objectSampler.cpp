/*
 * Copyright 2021 Andrei Pangin
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

#include <string.h>
#include "objectSampler.h"
#include "j9Ext.h"
#include "profiler.h"


u64 ObjectSampler::_interval;
volatile u64 ObjectSampler::_allocated_bytes;


void ObjectSampler::JavaObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                    jobject object, jclass object_klass, jlong size) {
    if (_enabled) {
        recordAllocation(jvmti, BCI_ALLOC, object_klass, size);
    }
}

void ObjectSampler::VMObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                  jobject object, jclass object_klass, jlong size) {
    if (_enabled) {
        recordAllocation(jvmti, BCI_ALLOC_OUTSIDE_TLAB, object_klass, size);
    }
}

void ObjectSampler::recordAllocation(jvmtiEnv* jvmti, int event_type, jclass object_klass, jlong size) {
    if (!updateCounter(_allocated_bytes, size, _interval)) {
        return;
    }

    AllocEvent event;
    event._class_id = 0;
    event._total_size = size;
    event._instance_size = size;

    char* class_name;
    if (jvmti->GetClassSignature(object_klass, &class_name, NULL) == 0) {
        if (class_name[0] == 'L') {
            event._class_id = Profiler::instance()->classMap()->lookup(class_name + 1, strlen(class_name) - 2);
        } else {
            event._class_id = Profiler::instance()->classMap()->lookup(class_name);
        }
        jvmti->Deallocate((unsigned char*)class_name);
    }

    Profiler::instance()->recordSample(NULL, size, event_type, &event);
}

Error ObjectSampler::check(Arguments& args) {
    if (J9Ext::InstrumentableObjectAlloc_id < 0) {
        return Error("InstrumentableObjectAlloc is not supported on this JVM");
    }
    return Error::OK;
}

Error ObjectSampler::start(Arguments& args) {
    Error error = check(args);
    if (error) {
        return error;
    }

    _interval = args._alloc > 1 ? args._alloc : 524287;
    _allocated_bytes = 0;

    jvmtiEnv* jvmti = VM::jvmti();
    if (jvmti->SetExtensionEventCallback(J9Ext::InstrumentableObjectAlloc_id, (jvmtiExtensionEvent)JavaObjectAlloc) != 0) {
        return Error("Could not enable InstrumentableObjectAlloc callback");
    }
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, NULL);

    return Error::OK;
}

void ObjectSampler::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, NULL);
    jvmti->SetExtensionEventCallback(J9Ext::InstrumentableObjectAlloc_id, NULL);
}
