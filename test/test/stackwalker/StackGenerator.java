/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.stackwalker;

public class StackGenerator {

    public static native double generateLargeFrame();

    public static native double generateDeepStack();

    public static native double generateCompleteStack();

    static {
        System.loadLibrary("jninativestacks");
    }

    public static void main(String[] args) throws InterruptedException {
        if (args.length != 1) {
            System.err.println("Usage: java Stackwalker <largeFrame|deepStack|completeStack>");
            System.exit(1);
        }

        if (args[0].equals("largeFrame")) {
            generateLargeFrame();
        } else if (args[0].equals("deepStack")) {
            generateDeepStack();
        } else if (args[0].equals("completeStack")) {
            generateCompleteStack();
        } else {
            System.err.println("Unknown test: " + args[0]);
            System.exit(1);
        }
    }
}
