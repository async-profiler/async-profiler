/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

public class CpuIntensiveApp {

    public static void main(String[] args) throws Exception {
        long start = System.currentTimeMillis();
        while (System.currentTimeMillis() - start < 12000) {
            for (int i = 0; i < 1000000; i++) {
                Math.sqrt(Math.random() * 1000000);
            }
        }
    }
}
