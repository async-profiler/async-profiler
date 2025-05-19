/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.comptask;

public class Main {
    private static final int LOOP_COUNT = 5;

    public static volatile double sink = 0;

    // This function should take a long time to compile
    public static void main(String[] args) {
        double result = 0;
        for (int a = 0; a < LOOP_COUNT; a++) {
            for (int b = 0; b < LOOP_COUNT; b++) {
                for (int c = 0; c < LOOP_COUNT; c++) {
                    for (int d = 0; d < LOOP_COUNT; d++) {
                        for (int e = 0; e < LOOP_COUNT; e++) {
                            for (int f = 0; f < LOOP_COUNT; f++) {
                                for (int g = 0; g < LOOP_COUNT; g++) {
                                    for (int h = 0; h < LOOP_COUNT; h++) {
                                        for (int i = 0; i < LOOP_COUNT; i++) {
                                            for (int j = 0; j < LOOP_COUNT; j++) {
                                                result += Math.sin(a) * Math.cos(j);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        sink += result;
    }
}
