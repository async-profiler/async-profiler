/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.instrument;

import one.profiler.*;

import java.time.Duration;
import java.util.Random;

public class CpuBurnerStopProfiler {

    private static void func1() {}
    
    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();

        // Phase 1: latency profiling
        profiler.execute("start,trace=test.instrument.CpuBurnerStopProfiler.func1");
        profiler.stop();

        // Phase 2: profile another method
        profiler.execute("start,trace=test.instrument.AnotherClass.func2");
        func1();
        String output = profiler.dumpCollapsed(Counter.SAMPLES);
        profiler.stop();

        assert output.isEmpty() : output;
    }
}

class AnotherClass {
    private static void func2() {}
}
