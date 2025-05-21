/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import java.lang.management.ManagementFactory;
import java.lang.reflect.Field;
import sun.misc.Unsafe;

public class Ttsp {
    private static final long RUN_DURATION_MS = 3_000;
    private static final long SAFEPOINT_INTERVAL_MS = 200;
    private static final long MEMORY_SIZE = 500 * 1024 * 1024;

    static void requestSafepoint() {
        ManagementFactory.getThreadMXBean().dumpAllThreads(false, false);
    }

    static Unsafe getUnsafe() {
        try {
            Field f = Unsafe.class.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            return (Unsafe) f.get(null);
        } catch (Exception exception) {
            throw new RuntimeException(exception);
        }
    }

    static void delaySafepoint() {
        Unsafe unsafe = getUnsafe();
        long address = unsafe.allocateMemory(MEMORY_SIZE);

        long value = 0;
        while (!Thread.interrupted()) {
            unsafe.setMemory(address, MEMORY_SIZE, (byte) (value++ % Byte.MAX_VALUE + 1));
        }

        unsafe.freeMemory(address);
    }

    public static void main(String[] args) throws Exception {
        long start = System.nanoTime();
        long end = start + RUN_DURATION_MS * 1_000_000;

        Thread safepointerDelayerThread = new Thread(Ttsp::delaySafepoint);
        safepointerDelayerThread.start();

        while (System.nanoTime() < end) {
            requestSafepoint();
            Thread.sleep(SAFEPOINT_INTERVAL_MS);
        }

        safepointerDelayerThread.interrupt();
        safepointerDelayerThread.join();
    }
}
