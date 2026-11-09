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
    static final long THIRD_SIZE = 4 * 1024 * 1024 + 17;
    static final long AFTER_FLUSH_SIZE = 5 * 1024 * 1024 + 17;

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        // Resolve the JNI methods before recording the allocation stacks under test.
        Native.free(Native.malloc(1));
        long retained = firstAllocation();
        long freed = freedAllocation();
        // Record distinct allocation stacks on both sides of a regular chunk switch.
        profiler.execute("dump,jfr");
        long afterFlush = afterFlushAllocation();
        profiler.execute("stop");

        // Each resume creates a new Recording and appends to the same file.
        profiler.execute("resume,nativemem,cstack=vm,file=" + args[0]);
        Native.free(freed);
        long other = secondAllocation();
        profiler.execute("stop");

        profiler.execute("resume,nativemem,cstack=vm,file=" + args[0]);
        long last = thirdAllocation();
        profiler.execute("stop");
        Native.free(retained);
        Native.free(afterFlush);
        Native.free(other);
        Native.free(last);

        // A fresh start resets the profiler dictionaries and records independently.
        profiler.execute("start,nativemem,cstack=vm,file=" + args[1]);
        retained = firstAllocation();
        profiler.execute("stop");
        Native.free(retained);
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

    private static long thirdAllocation() {
        return Native.malloc(THIRD_SIZE);
    }

    private static long afterFlushAllocation() {
        return Native.malloc(AFTER_FLUSH_SIZE);
    }
}
