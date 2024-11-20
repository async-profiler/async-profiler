/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativemem;

public class CallsMallocCalloc {
    static {
        System.loadLibrary("doesmalloc");
    }

    public static native long nativeMalloc(int size);

    public static native long nativeCalloc(int num, int size);

    public static void main(String[] args) throws InterruptedException {
        final boolean once = args.length > 0 && args[0].equals("once");

        final int malloc_size = 1999993; // unlikely, prime size.
        final int calloc_size = 2000147; // unlikely, prime size.

        do {
            nativeMalloc(malloc_size);
            nativeCalloc(1, calloc_size);

            // allocate every 1 second.
            Thread.sleep(1000);
        } while (!once);
    }
}
