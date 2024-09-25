/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

public class JfrCpuProfiling {
    private static volatile int value;

    private static void method1() {
        long startTime = System.currentTimeMillis();
        while (System.currentTimeMillis() - startTime < 10) {
            value +=  System.getProperties().hashCode();
        }
    }

    public static void main(String[] args) throws Exception {
        while (true) {
            method1();
        }
    }
}
