/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.smoke;

import java.util.Random;
import java.util.concurrent.ThreadLocalRandom;

public class Alloc implements Runnable {
    static volatile Object sink;

    public static void main(String[] args) {
        new Thread(new Alloc(), "AllocThread-1").start();
        new Thread(new Alloc(), "AllocThread-2").start();
    }

    @Override
    public void run() {
        Random random = ThreadLocalRandom.current();
        while (true) {
            allocate(random);
        }
    }

    private static void allocate(Random random) {
        if (random.nextBoolean()) {
            sink = new int[128 * 1000];
        } else {
            sink = new Integer[128 * 1000];
        }
    }
}
