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

        // Call trace storage should contain at least the initial hash table allocation
        // LongHashTable header (144 bytes) + 65536 entries × (8 bytes key + 24 bytes CallTraceSample)
        // Total: 144 + 65536 × 32 = 2,097,296 bytes, rounded up to page boundary (~2.1MB)
        long minCallTraceStorage = 2097296;
        assert metrics1[0] >= minCallTraceStorage;

        // Code cache should contain at least the basic memory management functions:
        // calloc(6) + dlopen(6) + free(4) + malloc(6) + realloc(7) = 29 chars
        // Each NativeFunc: sizeof(NativeFunc) + 1 + strlen(name) = 4 + 1 + len = 5 + len
        // Total: 5*5 + 29 = 54 bytes minimum for these 5 functions
        long minExpected = 54;
        assert metrics1[3] >= minExpected;
        
        for (int i = 0; i < 5; i++) {
            assert metrics1[i] >= 0;
        }
        
        profiler.start("cpu,file=profile.jfr,jfr", 1000000);
        
        for (int i = 0; i < 1000000; i++) {
            Math.sqrt(i);
            CpuBurner.burn();
        }
        Thread.sleep(10000);
        profiler.stop();
        
        long[] metrics2 = new long[5];
        profiler.getMetrics(metrics2);
        
        assert metrics2[0] >= metrics1[0];
        assert metrics2[1] >= metrics1[1];

    }
}
