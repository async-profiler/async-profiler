/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.TimeUnit;

/**
 * Process to simulate lock contention and allocate objects.
 */
public class JfrMultiModeProfiling {
    private static final Object lock = new Object();

    private static volatile Object sink;
    private static int count = 0;
    private static final List<byte[]> holder = new ArrayList<>();

    private static final ThreadMXBean tmx = ManagementFactory.getThreadMXBean();
    private static final Map<Long, Boolean> threadIds = new ConcurrentHashMap<>();

    static {
        tmx.setThreadContentionMonitoringEnabled(true);
    }

    public static void main(String[] args) throws InterruptedException {
        ExecutorService executor = Executors.newFixedThreadPool(2);
        List<CompletableFuture<Void>> futures = new ArrayList<>();

        for (int i = 0; i < 10; i++) {
            futures.add(CompletableFuture.runAsync(JfrMultiModeProfiling::cpuIntensiveIncrement, executor));
        }
        allocate();
        futures.forEach(CompletableFuture::join);

        long[] ids = threadIds.keySet().stream().mapToLong(Long::longValue).toArray();
        Arrays.stream(tmx.getThreadInfo(ids)).mapToLong(ThreadInfo::getBlockedTime).forEach(System.out::println);

        executor.shutdown();
        executor.awaitTermination(10, TimeUnit.SECONDS);
    }

    private static void cpuIntensiveIncrement() {
        threadIds.putIfAbsent(Thread.currentThread().getId(), Boolean.TRUE);

        for (int i = 0; i < 100_000; i++) {
            synchronized (lock) {
                count += System.getProperties().hashCode();
            }
        }
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
