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
 * Run with `-f profile.jfr`
 * <p>
 * The program generates a continuous load with
 * the peaks of CPU usage every 5 seconds.
 * The analysis of JFR output in JDK Mission Control
 * shows the cause of the peaks.
 */
public class RegularPeak {
    private static final int SIZE = 100_000;

    private final Cache cache = new Cache();

    public static void main(String[] args) throws Exception {
        RegularPeak test = new RegularPeak();

        while (true) {
            test.run();
            Thread.sleep(20);
        }
    }

    public RegularPeak() {
        for (long id = 0; id < SIZE; id++) {
            cache.put(id, Long.toString(ThreadLocalRandom.current().nextLong()));
        }
    }

    private void run() {
        long count = LongStream.range(0, SIZE)
                .mapToObj(cache::get)
                .filter(Objects::nonNull)
                .count();
    }
}