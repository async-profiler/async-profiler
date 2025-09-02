/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.otlp;

import one.profiler.AsyncProfiler;

public class MetricsTest {
    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        
        int[] metrics1 = profiler.getMetrics();
        System.out.println("Before profiling:");
        System.out.println("Call trace storage: " + metrics1[0] + " KB");
        System.out.println("Flight recording: " + metrics1[1] + " KB");
        System.out.println("Dictionaries: " + metrics1[2] + " KB");
        System.out.println("Code cache: " + metrics1[3] + " KB");
        System.out.println("Discarded samples: " + metrics1[4]);
        
        profiler.start("cpu,file=profile.jfr,jfr", 1000000);
        
        for (int i = 0; i < 1000000; i++) {
            Math.sqrt(i);
            CpuBurner.burn();
        }
        Thread.sleep(10000);
        profiler.stop();
        
        int[] metrics2 = profiler.getMetrics();
        System.out.println("\nAfter profiling:");
        System.out.println("Call trace storage: " + metrics2[0] + " KB");
        System.out.println("Flight recording: " + metrics2[1] + " KB");
        System.out.println("Dictionaries: " + metrics2[2] + " KB");
        System.out.println("Code cache: " + metrics2[3] + " KB");
        System.out.println("Discarded samples: " + metrics2[4]);
        
        assert metrics2[0] >= metrics1[0];
        assert metrics2[1] >= metrics1[1];
    }
}
