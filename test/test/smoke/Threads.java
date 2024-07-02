/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.smoke;

import java.math.BigInteger;

public class Threads {

    public static void main(String[] args) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                methodForThreadEarlyEnd();
            }
        }, "ThreadEarlyEnd").start();

        new Thread(new Runnable() {
            @Override
            public void run() {
                Thread.currentThread().setName("RenamedThread");
                methodForRenamedThread();
            }
        }, "ThreadWillBeRenamed").start();
    }

    static void methodForThreadEarlyEnd() {
        long now = System.currentTimeMillis();
        BigInteger counter = BigInteger.ZERO;
        while (System.currentTimeMillis() - now < 300) {
            counter = counter.nextProbablePrime();
        }
    }

    static void methodForRenamedThread() {
        long now = System.currentTimeMillis();
        BigInteger counter = BigInteger.ZERO;
        while (System.currentTimeMillis() - now < 1000) {
            counter = counter.nextProbablePrime();
        }
    }
}
