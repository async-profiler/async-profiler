/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.stackwalker;

public class StackGenerator {

    public static native double largeFrame();

    public static native double deepFrame();

    public static native double leafFrame();

    static {
        System.loadLibrary("jninativestacks");
    }

    public static void main(String[] args) {
        if (args.length != 1) {
            System.err.println("Usage: java StackGenerator <largeFrame|deepFrame|leafFrame>");
            System.exit(1);
        }

        if (args[0].equals("largeFrame")) {
            largeFrame();
        } else if (args[0].equals("deepFrame")) {
            deepFrame();
        } else if (args[0].equals("leafFrame")) {
            leafFrame();
        } else {
            System.err.println("Unknown test: " + args[0]);
            System.exit(1);
        }
    }
}
