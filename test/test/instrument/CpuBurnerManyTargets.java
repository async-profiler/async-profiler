/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.instrument;

import java.time.Duration;
import java.util.Random;

public class CpuBurnerManyTargets {
    
    static void burn1(Duration duration) {
        CpuBurner.burn(duration);
    }

    static void burn2(Duration duration) {
        CpuBurner.burn(duration);
    }

    public static void main(String[] args) throws InterruptedException {
        Thread t1 = new Thread(() -> burn1(Duration.ofMillis(50)), "thread1");
        Thread t2 = new Thread(() -> burn2(Duration.ofMillis(10)), "thread2");
        t1.start();
        t2.start();
    }
}
