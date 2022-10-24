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
#include "vmEntry.h"
#include "os.h"

static const Context* EMPTY = new Context {0, 0, 0, 0, 0, 0, 0, 0};
Context** Contexts::_pages = new Context *[Contexts::getMaxPages()];

const Context& Contexts::get(int tid) {
    int pageIndex = tid / PAGE_SIZE;
    Context* page = _pages[pageIndex];
    if (page == NULL) {
        return *EMPTY;
    }
    return page[tid % PAGE_SIZE];
}

bool Contexts::isValid(const Context &context) {
    return (context.spanId ^ context.rootSpanId) == context.checksum;
}

void Contexts::initialize(int pageIndex) {
    if (__atomic_load_n(&_pages[pageIndex], __ATOMIC_ACQUIRE) == NULL) {
        int capacity = PAGE_SIZE * sizeof(Context);
        Context *page = (Context*) aligned_alloc(sizeof(Context), capacity);
        if (!__sync_bool_compare_and_swap(&_pages[pageIndex], NULL, page)) {
            free(page);
        } else {
            // need to zero the storage because there is no aligned_calloc
            memset(page, 0, capacity);
        }
    }
}

ContextPage Contexts::getPage(int tid) {
    int pageIndex = tid / PAGE_SIZE;
    initialize(pageIndex);
    return {.capacity = PAGE_SIZE * sizeof(Context), .storage = _pages[pageIndex]};
}

int Contexts::getMaxPages() {
    return OS::getMaxThreadId() / PAGE_SIZE;
}
