/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.recovery;

public class TimeLoop {

    public static void main(String[] args) throws InterruptedException {
        long busyTime = 100;
        long idleTime = 100;

        while (true) {
            long startTime = System.currentTimeMillis();
            while (System.currentTimeMillis() - startTime < busyTime) {
                // Burn CPU
            }
            Thread.sleep(idleTime);
        }
    }
}
