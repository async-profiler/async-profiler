/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.smoke;

import java.io.File;

public class Cpu {
    private static volatile int value;

    private static void method1() {
        for (int i = 0; i < 1000000; i++) {
            value++;
        }
    }

    private static void method2() {
        for (int i = 0; i < 1000000; i++) {
            value++;
        }
    }

    private static void method3() throws Exception {
        long startTime = System.currentTimeMillis();
        while (System.currentTimeMillis() - startTime < 10) {
            for (String s : new File("/tmp").list()) {
                value += s.hashCode();
            }
        }
    }

    public static void main(String[] args) throws Exception {
        System.out.println("Starting...");
        while (true) {
            method1();
            method2();
            method3();
        }
    }
}
