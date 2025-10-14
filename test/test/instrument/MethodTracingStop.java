/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.instrument;

import one.profiler.*;

import java.time.Duration;
import java.util.Random;

public class MethodTracingStop {

    private static void func1() {}
    
    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();

        // Phase 1: latency profiling
        profiler.execute("start,trace=test.instrument.MethodTracingStop.func1");
        profiler.stop();

        // Phase 2: profile another method
        profiler.execute("start,trace=test.instrument.AnotherClass.func2");
        func1();
        String output = profiler.dumpCollapsed(Counter.SAMPLES);
        profiler.stop();

        // If output is not empty, the instrumentation from the previous
        // call wasn't properly cleared. That's wrong.
        assert output.isEmpty() : output;
    }
}
