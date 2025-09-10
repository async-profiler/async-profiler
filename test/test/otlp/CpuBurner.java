/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.otlp;

import java.lang.Thread;
import java.time.Duration;
import java.time.Instant;
import java.util.Random;

public class CpuBurner {
    private static final Random RANDOM = new Random();
    private static final Duration TEST_DURATION = Duration.ofSeconds(1);

    static void burn() {
        long n = RANDOM.nextLong();
        if (Long.toString(n).hashCode() == 0) {
            System.out.println(n);
        }
    }

    public static void main(String[] args) throws InterruptedException {
        Thread worker = new Thread(() -> {
            Instant start = Instant.now();
            while (Duration.between(start, Instant.now()).compareTo(TEST_DURATION) < 0) {
                burn();
            }
        }, "CpuBurnerWorker");
        worker.start();
        worker.join();
    }
}
