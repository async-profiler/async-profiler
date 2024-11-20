/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativemem;

public class CallsRealloc {
    static {
        System.loadLibrary("doesmalloc");
    }

    public static native long nativeMalloc(int size);

    public static native long nativeRealloc(long addr, int size);

    public static void main(String[] args) throws InterruptedException {
        final boolean once = args.length > 0 && args[0].equals("once");

        final int malloc_size = 1999993; // unlikely, prime size.
        final int realloc_size = 30000170;// unlikely, prime size.

        do {
            long addr = nativeMalloc(malloc_size);
            long reallocd = nativeRealloc(addr, realloc_size);

            // allocate every 1 second.
            Thread.sleep(1000);
        } while (!once);
    }
}
