/*
 * Copyright 2022 Andrei Pangin
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
#include "j9Ext.h"
#include "j9ObjectSampler.h"
#include <string.h>


jvmtiEnv* J9Ext::_jvmti;

void* (*J9Ext::_j9thread_self)() = NULL;

jvmtiExtensionFunction J9Ext::_GetOSThreadID = NULL;
jvmtiExtensionFunction J9Ext::_GetJ9vmThread = NULL;
jvmtiExtensionFunction J9Ext::_GetStackTraceExtended = NULL;
jvmtiExtensionFunction J9Ext::_GetAllStackTracesExtended = NULL;

int J9Ext::InstrumentableObjectAlloc_id = -1;


// Look for OpenJ9-specific JVM TI extension
bool J9Ext::initialize(jvmtiEnv* jvmti, const void* j9thread_self) {
    _jvmti = jvmti;
    _j9thread_self = (void* (*)())j9thread_self;

    jint ext_count;
    jvmtiExtensionFunctionInfo* ext_functions;
    if (jvmti->GetExtensionFunctions(&ext_count, &ext_functions) == 0) {
        for (int i = 0; i < ext_count; i++) {
            if (strcmp(ext_functions[i].id, "com.ibm.GetOSThreadID") == 0) {
                _GetOSThreadID = ext_functions[i].func;
            } else if (strcmp(ext_functions[i].id, "com.ibm.GetJ9vmThread") == 0) {
                _GetJ9vmThread = ext_functions[i].func;
            } else if (strcmp(ext_functions[i].id, "com.ibm.GetStackTraceExtended") == 0) {
                _GetStackTraceExtended = ext_functions[i].func;
            } else if (strcmp(ext_functions[i].id, "com.ibm.GetAllStackTracesExtended") == 0) {
                _GetAllStackTracesExtended = ext_functions[i].func;
            }
        }
       jvmti->Deallocate((unsigned char*)ext_functions);
    }

    jvmtiExtensionEventInfo* ext_events;
    if (jvmti->GetExtensionEvents(&ext_count, &ext_events) == 0) {
        for (int i = 0; i < ext_count; i++) {
            if (strcmp(ext_events[i].id, "com.ibm.InstrumentableObjectAlloc") == 0) {
                InstrumentableObjectAlloc_id = ext_events[i].extension_event_index;
                // If we don't set a callback now, we won't be able to enable it later in runtime
                jvmti->SetExtensionEventCallback(InstrumentableObjectAlloc_id, (jvmtiExtensionEvent)J9ObjectSampler::JavaObjectAlloc);
                jvmti->SetExtensionEventCallback(InstrumentableObjectAlloc_id, NULL);
                break;
            }
        }
       jvmti->Deallocate((unsigned char*)ext_events);
    }

    return _GetOSThreadID != NULL && _GetStackTraceExtended != NULL && _GetAllStackTracesExtended != NULL;
}
