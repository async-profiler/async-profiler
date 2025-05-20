/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.comptask;

public class Main {
    private static final long RUN_DURATION_MS = 1000;

    public static void main(String[] args) throws Exception {
        long start = System.nanoTime();
        long end = start + RUN_DURATION_MS * 1_000_000;
        while (System.nanoTime() < end) {}
    }
}
