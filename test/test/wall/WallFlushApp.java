/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.wall;

import java.util.concurrent.CountDownLatch;

import one.profiler.AsyncProfiler;

public class WallFlushApp {

    public static void main(String[] args) throws Exception {
        CountDownLatch release = new CountDownLatch(1);
        Thread worker = new Thread(() -> {
            try {
                release.await();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }, "WallFlushSleeper");
        worker.start();

        Thread.sleep(1000);

        AsyncProfiler.getInstance().execute("dump,jfr");

        release.countDown();
        worker.join();
    }
}
