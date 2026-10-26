/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativemem;

import one.profiler.AsyncProfiler;

public class CallsMallocAcrossChunks {
    static final long FIRST_SIZE = 1024 * 1024 + 17;
    static final long FREED_SIZE = 2 * 1024 * 1024 + 17;
    static final long SECOND_SIZE = 3 * 1024 * 1024 + 17;

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        // Resolve the JNI methods before recording the allocation stacks under test.
        Native.free(Native.malloc(1));
        Thread.currentThread().setName("first-chunk");
        long retained = firstAllocation();
        long freed = freedAllocation();
        profiler.execute("stop");

        Thread.currentThread().setName("second-chunk");
        // Resume appends a real chunk with a new recording-local method dictionary.
        profiler.execute("resume,nativemem,cstack=vm,file=" + args[0]);
        Native.free(freed);
        long other = secondAllocation();
        profiler.execute("stop");
        Native.free(retained);
        Native.free(other);
    }

    private static long firstAllocation() {
        return Native.malloc(FIRST_SIZE);
    }

    private static long freedAllocation() {
        return Native.malloc(FREED_SIZE);
    }

    private static long secondAllocation() {
        return Native.malloc(SECOND_SIZE);
    }
}
