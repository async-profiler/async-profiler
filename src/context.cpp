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

bool Contexts::_wall_filtering = true;
bool Contexts::_cpu_filtering = true;

Context Contexts::get(int tid) {
    return _contexts[tid];
}

bool Contexts::filter(Context ctx, int event_type) {
    switch (event_type) {
    case BCI_WALL:
        return !_wall_filtering || ctx.spanId > 0;
    case BCI_CPU:
        return !_cpu_filtering || ctx.spanId > 0;
    default:
        // no filtering based on context
        return true;
    }
}

bool Contexts::filter(int tid, int event_type) {
    // the thread should be suspended, so _contexts[tid] shouldn't change
    Context context = _contexts[tid];
    return filter(context, event_type);
}

void Contexts::set(int tid, Context context) {
    _contexts[tid] = context;
}

void Contexts::clear(int tid) {
    _contexts[tid].spanId = 0;
    _contexts[tid].rootSpanId = 0;
}

void Contexts::initialize() {
    if (__atomic_load_n(&_contexts, __ATOMIC_ACQUIRE) == NULL) {
        Context *contexts = (Context*)calloc(_contexts_size = OS::getMaxThreadId(), sizeof(Context));
        if (!__sync_bool_compare_and_swap(&_contexts, NULL, contexts)) {
            free(contexts);
        }
    }
}

ContextStorage Contexts::getStorage() {
    initialize();
    return {.capacity = _contexts_size * (int) sizeof(Context), .storage = _contexts};
}
