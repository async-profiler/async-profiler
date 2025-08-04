/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Random;
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
    private static final Map<Long, ThreadInformation> threadInformation = new ConcurrentHashMap<>();

    static {
        tmx.setThreadContentionMonitoringEnabled(true);
    }

    public static void main(String[] args) throws InterruptedException {
        ExecutorService executor = Executors.newFixedThreadPool(2);
        for (int i = 0; i < 10; i++) {
            executor.submit(JfrMultiModeProfiling::cpuIntensiveIncrement);
        }
        allocate();
        executor.shutdown();
        executor.awaitTermination(10, TimeUnit.SECONDS);

        System.out.println(threadInformation.values().stream().mapToLong(info -> info.blockedTime).sum());
        System.out.println(threadInformation.values().stream().mapToLong(info -> info.blockedCount).sum());
    }

    private static void cpuIntensiveIncrement() {
        for (int i = 0; i < 100_000; i++) {
            synchronized (lock) {
                count += System.getProperties().hashCode();
            }
        }

        long threadId = Thread.currentThread().getId();
        threadInformation.put(threadId, new ThreadInformation(tmx.getThreadInfo(threadId)));
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

    static class ThreadInformation {
        long blockedTime;
        long blockedCount;

        ThreadInformation(ThreadInfo threadInfo) {
            blockedTime = threadInfo.getBlockedTime();
            blockedCount = threadInfo.getBlockedCount();
        }
    }
}
