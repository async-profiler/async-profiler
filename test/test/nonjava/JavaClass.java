/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nonjava;

public class JavaClass {

    public static double cpuHeavyTask() {
        double sum = 0;
        for (int i = 0; i < 100000; i++) {
            sum += Math.sqrt(Math.random());
            sum += Math.pow(Math.random(), Math.random());
        }
        return sum;
    }
}
