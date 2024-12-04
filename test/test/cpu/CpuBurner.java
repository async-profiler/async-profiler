/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.cpu;

import java.util.Random;

public class CpuBurner {
    private static final Random random = new Random();

    static void burn() {
        long n = random.nextLong();
        if (Long.toString(n).hashCode() == 0) {
            System.out.println(n);
        }
    }

    public static void main(String[] args) {
        while (true) {
            burn();
        }
    }
}
