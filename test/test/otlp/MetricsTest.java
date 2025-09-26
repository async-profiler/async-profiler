/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.otlp;

import one.profiler.AsyncProfiler;

public class MetricsTest {
    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        
        long[] metrics1 = new long[5];
        profiler.getMetrics(metrics1);
        
        for (int i = 0; i < 5; i++) {
            assert metrics1[i] >= 0;
        }
        
        profiler.start("cpu,file=profile.jfr,jfr", 1000000);
        
        for (int i = 0; i < 1000000; i++) {
            Math.sqrt(i);
            CpuBurner.burn();
        }
        profiler.stop();
        
        long[] metrics2 = new long[5];
        profiler.getMetrics(metrics2);
        
        assert metrics2[0] >= metrics1[0];
        assert metrics2[1] >= metrics1[1];
    }
}
