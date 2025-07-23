/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import java.util.Objects;
import java.util.concurrent.ThreadLocalRandom;
import java.util.stream.LongStream;

/**
 * This example makes use of JFR compatible output.
 * Run with `file=profile.jfr`
 * <p>
 * The program generates a continuous load with
 * the peaks of CPU usage every 4 seconds.
 * The analysis of JFR output in JDK Mission Control
 * shows the cause of the peaks.
 */
public class RegularPeak {
    private static final int SIZE = 100_000;

    private static final Cache cache = new Cache();

    static {
        for (long id = 0; id < SIZE; id++) {
            cache.put(id, Long.toString(ThreadLocalRandom.current().nextLong()));
        }
    }

    private static void consistentCpuLoad() {
        long count = 0;

        while (true) {
            count += LongStream.range(0, SIZE)
                    .mapToObj(cache::get)
                    .filter(Objects::nonNull)
                    .count();
            try {
                Thread.sleep(20);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    public static void main(String[] args) throws Exception {
        Thread thread = new Thread(RegularPeak::consistentCpuLoad);
        thread.start();

        Thread.sleep(4000);
        cache.calculateTop();

        Thread.sleep(2000);

        System.exit(0);
    }
}