/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <stdlib.h>
 #include <string.h>

 __attribute__((visibility("default")))
 void* malloc(size_t size) {
     void* ptr = calloc(1, size);
     memset(ptr, 0xFF, size);
     return ptr;
 }
 