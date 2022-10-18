/*
 * Copyright 2021, 2022 Datadog, Inc
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

#include "context.h"
#include "os.h"
#include "vmEntry.h"

int Contexts::_contexts_size = -1;
Context* Contexts::_contexts = NULL;

Context Contexts::get(int tid) {
    return _contexts[tid];
}

void Contexts::initialize() {
    if (__atomic_load_n(&_contexts, __ATOMIC_ACQUIRE) == NULL) {
        _contexts_size = OS::getMaxThreadId();
        int capacity = _contexts_size * sizeof(Context);
        Context *contexts = (Context*) aligned_alloc(sizeof(Context), capacity);
        if (!__sync_bool_compare_and_swap(&_contexts, NULL, contexts)) {
            free(contexts);
        } else {
            // need to zero the storage because there is no aligned_calloc
            memset(contexts, 0, _contexts_size * sizeof(Context) / sizeof(int));
        }
    }
}

ContextStorage Contexts::getStorage() {
    initialize();
    return {.capacity = _contexts_size * (int) sizeof(Context), .storage = _contexts};
}
