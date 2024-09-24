/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.parser;

import java.io.File;

public class Cpu {
    private static volatile int value;

    private static void method1() throws Exception {
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
        }
    }
}
