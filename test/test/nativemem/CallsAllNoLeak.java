/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativemem;

public class CallsAllNoLeak {

    private static final int NUM_THREADS = 8; // Number of threads

    private static final int MALLOC_SIZE = 1999993; // Prime size, useful in assertions.
    private static final int CALLOC_SIZE = 2000147;
    private static final int REALLOC_SIZE = 30000170;

    private static void do_work(boolean once) {
        try {
            do {
                long addr = Native.malloc(MALLOC_SIZE);
                long reallocd = Native.realloc(addr, REALLOC_SIZE);
                Native.free(reallocd);

                long callocd = Native.calloc(1, CALLOC_SIZE);
                Native.free(callocd);

                Thread.sleep(1);
            } while (!once);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            System.err.println("Thread interrupted: " + Thread.currentThread().getName());
        }
    }

    public static void main(String[] args) throws InterruptedException {
        final boolean once = args.length > 0 && args[0].equals("once");

        final Thread[] threads = new Thread[NUM_THREADS];
        for (int i = 0; i < NUM_THREADS; i++) {
            threads[i] = new Thread(() -> do_work(once), "MemoryTask-" + i);
            threads[i].start();
        }

        for (Thread thread : threads) {
            thread.join();
        }
    }
}
