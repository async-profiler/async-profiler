/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>

const int MALLOC_SIZE = 1999993;
const int MALLOC_DYN_SIZE = 2000003;

// A global pointer referencing malloc as data -> .rela.dyn
static void* (*malloc_dyn)(size_t) = malloc;

int main(void) {
    // Direct call -> .rela.plt
    void* p = malloc(MALLOC_SIZE);

    void* q = malloc_dyn(MALLOC_DYN_SIZE);

    free(p);
    free(q);

    return 0;
}
