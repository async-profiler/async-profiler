/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.pmu;

import java.util.concurrent.ThreadLocalRandom;

/**
 * This demo shows the importance of hardware performance counters.
 * Two tests (test16K and test8M) execute the same number of
 * operations, however, test16K completes much quicker than test8M.
 * <p>
 * CPU profiling shows no difference between two tests,
 * but cache-misses profiling highlights test8M
 * as the problematic method.
 */
public class Dictionary {

    private static void testRandomRead(long[] array, int bound) {
        long startTime = System.nanoTime();

        for (long i = 0; i < Integer.MAX_VALUE; i++) {
            int index = ThreadLocalRandom.current().nextInt(bound);
            array[index]++;
        }

        long endTime = System.nanoTime();
        System.out.printf("Time spent: %.3f\n", (endTime - startTime) / 1e9);
    }

    public static void test16K() {
        long[] array = new long[8 * 1024 * 1024];
        testRandomRead(array, 16384);
    }

    public static void test8M() {
        long[] array = new long[8 * 1024 * 1024];
        testRandomRead(array, 8 * 1024 * 1024);
    }

    public static void main(String[] args) {
        new Thread(Dictionary::test16K).start();
        new Thread(Dictionary::test8M).start();
    }
}
