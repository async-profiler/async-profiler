/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

import one.profiler.AsyncProfiler;

import java.util.concurrent.locks.LockSupport;

public class PacedAlloc {
    private static volatile Object sink;

    private static void allocate(long millis) {
        long start = System.nanoTime();
        for (long ms = 1; ms <= millis; ms++) {
            for (int i = 0; i < 24; i++) {
                sink = new byte[112];
            }
            long remaining = start + ms * 1_000_000 - System.nanoTime();
            if (remaining > 0) {
                LockSupport.parkNanos(remaining);
            }
        }
    }

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();

        profiler.execute("start,alloc=1k,ratelimit=alloc:0,file=" + args[0]);
        for (int i = 0; i < 1000; i++) {
            sink = new byte[16 * 1024];
        }
        profiler.execute("stop");

        profiler.execute("start,alloc=1k,ratelimit=alloc:2000,file=" + args[1]);
        allocate(2000);
        profiler.execute("stop");
    }
}
