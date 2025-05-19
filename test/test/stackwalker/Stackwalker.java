/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.stackwalker;

public class Stackwalker {

    public static native void walkStackLargeFrame();

    public static native void walkStackDeepStack();

    public static native void walkStackComplete();

    static {
        System.loadLibrary("jnistackwalker");
    }

    public static void main(String[] args) throws InterruptedException {
        if (args.length != 1) {
            System.err.println("Usage: java Stackwalker <walkStackLargeFrame|walkStackDeepStack|walkStackComplete>");
            System.exit(1);
        }

        if (args[0].equals("walkStackLargeFrame")) {
            walkStackLargeFrame();
        } else if (args[0].equals("walkStackDeepStack")) {
            walkStackDeepStack();
        } else if (args[0].equals("walkStackComplete")) {
            walkStackComplete();
        } else {
            System.err.println("Unknown test: " + args[0]);
            System.exit(1);
        }
    }
}
