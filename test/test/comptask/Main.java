/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.comptask;

public class Main {
    public static volatile double sink = 0;

    public static void main(String[] args) throws Exception {
        // Simulate some CPU-bound work
        double result = 0;
        for (int j = 0; j < 1000000; j++) {
            result += Math.sin(j) * Math.cos(j);
        }
        sink += result;
    }
}
