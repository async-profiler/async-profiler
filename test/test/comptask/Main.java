/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.comptask;

public class Main {
    private static final long RUN_DURATION_MS = 1000;

    public static volatile double sink = 0;

    // This function should take a long time to compile
    public static void main(String[] args) {
        long start = System.nanoTime();
        long end = start + RUN_DURATION_MS * 1_000_000;
        while (System.nanoTime() < end) {
            double result = 0;
            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;

            for (int j = 0; j < 5; j++) {
                result += Math.sin(j) * Math.cos(j);
            }
            sink += result;
        }
    }
}
