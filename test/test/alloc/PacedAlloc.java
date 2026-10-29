/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

import one.profiler.AsyncProfiler;

import java.util.concurrent.locks.LockSupport;

public class PacedAlloc {
    private static volatile Object sink;

    public static void main(String[] args) throws Exception {
        AsyncProfiler.getInstance().execute("start,alloc=1k");
        for (int i = 0; i < 1000; i++) {
            sink = new byte[16 * 1024];
        }
        AsyncProfiler.getInstance().execute("stop");

        AsyncProfiler.getInstance().execute("start,alloc=1k,ratelimit=alloc:1000/250,file=" + args[0]);
        long start = System.nanoTime();
        for (long ms = 1; ms <= 1000; ms++) {
            for (int i = 0; i < 96; i++) {
                sink = new byte[112];
            }
            LockSupport.parkNanos(start + ms * 1_000_000 - System.nanoTime());
        }
        AsyncProfiler.getInstance().execute("stop");
    }
}
