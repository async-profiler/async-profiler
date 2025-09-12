/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

import java.util.concurrent.CountDownLatch;

public class MultiThreadApp {
    public static void main(String[] args) throws Exception {
        int threadCount = 8;
        CountDownLatch latch = new CountDownLatch(threadCount);

        for (int i = 0; i < threadCount; i++) {
            final int threadId = i;
            new Thread(() -> {
                try {

                    long start = System.currentTimeMillis();
                    while (System.currentTimeMillis() - start < 12000) {
                        for (int j = 0; j < 1000000; j++) {
                            Math.sqrt(Math.random() * 1000000);
                        }
                    }

                    Thread.sleep(8000);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                } finally {
                    latch.countDown();
                }
            }, "WorkerThread-" + threadId).start();
        }

        latch.await();
    }
}
