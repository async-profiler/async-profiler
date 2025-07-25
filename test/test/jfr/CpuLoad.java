/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

public class CpuLoad {

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    private static long cpuLoad() {
        long count = 0;
        for (int i = 0; i < 100_000; i++) {
            count += System.getProperties().hashCode();
        }
        return count;
    }

    private static void consistentCpuLoad() {
        long startTime = System.currentTimeMillis();

        while (System.currentTimeMillis() - startTime < 4000) {
            cpuLoad();
            sleep(20);
        }
    }

    private static void inconsistentCpuLoad() {
        sleep(2000);

        for (int i = 0; i < 10; i++) {
            cpuLoad();
        }
    }

    public static void main(String[] args) throws Exception {
        Thread thread = new Thread(CpuLoad::consistentCpuLoad);
        thread.start();

        inconsistentCpuLoad();
        thread.join();
    }
}
