/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Random;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadLocalRandom;

/**
 * Process to simulate lock contention and allocate objects.
 */
public class JfrMultiModeProfiling {
    private static final Object lock = new Object();

    private static volatile Object sink;
    private static int count = 0;
    private static final List<byte[]> holder = new ArrayList<>();

    public static void main(String[] args) throws InterruptedException {
        ExecutorService executor = Executors.newFixedThreadPool(2);
        List<CompletableFuture<Long>> completableFutures = new ArrayList<>();
        long startTime = System.nanoTime();
        for (int i = 0; i < 10; i++) {
            completableFutures.add(CompletableFuture.supplyAsync(JfrMultiModeProfiling::cpuIntensiveIncrement, executor));
        }
        allocate();
        long endTime = completableFutures.stream().map(CompletableFuture::join).max(Long::compareTo).get();
        System.out.println(endTime - startTime);
        executor.shutdown();
    }

    private static long cpuIntensiveIncrement() {
        for (int i = 0; i < 100_000; i++) {
            synchronized (lock) {
                count += System.getProperties().hashCode();
            }
        }
        return System.nanoTime();
    }

    private static void allocate() {
        long start = System.currentTimeMillis();
        Random random = ThreadLocalRandom.current();
        while (System.currentTimeMillis() - start <= 1000) {
            if (random.nextBoolean()) {
                sink = new byte[65536];
            } else {
                sink = String.format("some string: %s, some number: %d", new Date(), random.nextInt());
            }
            if (holder.size() < 100_000) {
                holder.add(new byte[1]);
            }
        }
    }
}
