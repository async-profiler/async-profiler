/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.wall;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicLong;

class PingPongClient {
    private static final ExecutorService executor = Executors.newCachedThreadPool();
    public static void main(String[] args) throws Exception {
        long result = 0;
        for (int i = 0; i < 10; i++) {
            result += pingPong();
        }
    }

    private static long pingPong() {
        Object monitor = new Object();
        AtomicLong counter = new AtomicLong();
        long startTime = System.currentTimeMillis();
        List<CompletableFuture<Void>> futures = new ArrayList<>();
        for (int i = 0; i < 2; i++) {
            futures.add(CompletableFuture.supplyAsync(() -> {
                        while (System.currentTimeMillis() - startTime < 500) {
                            synchronized (monitor) {
                                counter.addAndGet(busyWork(Duration.ofMillis(100)));
                            }
                            counter.addAndGet(busyWork(Duration.ofMillis(10)));
                        }
                        return null;
                    },
                    executor));
        }

        CompletableFuture.allOf(futures.toArray(new CompletableFuture[0])).join();
        return counter.get();
    }

    private static long busyWork(Duration duration) {
        long startTime = System.nanoTime();
        long counter = ThreadLocalRandom.current().nextLong();
        while (System.nanoTime() - startTime < duration.toNanos()) {
            counter ^= ThreadLocalRandom.current().nextLong();
        }
        return counter;
    }
}
