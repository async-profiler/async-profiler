/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.instrument;

import java.time.Duration;
import java.util.Random;

public class CpuBurner {
    private static final Random random = new Random();

    static void burn(Duration duration) {
        long start = System.nanoTime();
        while (System.nanoTime() - start < duration.toNanos()) {
            long n = random.nextLong();
            if (Long.toString(n).hashCode() == 0) {
                System.out.println(n);
            }
        }
    }

    public static void main(String[] args) throws InterruptedException {
        Thread t1 = new Thread(() -> {
            burn(Duration.ofMillis(500));
            burn(Duration.ofMillis(10));
        }, "thread1");
        Thread t2 = new Thread(() -> {
            burn(Duration.ofMillis(300));
            burn(Duration.ofMillis(30));
            burn(Duration.ofMillis(150));
        }, "thread2");
        Thread t3 = new Thread(() -> burn(Duration.ofMillis(50)), "thread3");
        Thread t4 = new Thread(() -> burn(Duration.ofMillis(10)), "thread4");
        t1.start();
        t2.start();
        t3.start();
        t4.start();
        t1.join();
        t2.join();
        t3.join();
        t4.join();
    }
}
