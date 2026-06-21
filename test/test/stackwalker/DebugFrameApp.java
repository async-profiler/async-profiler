/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.stackwalker;

public class DebugFrameApp {

    public static native double run(int iterations);

    static {
        System.loadLibrary("debugframe");
    }

    public static void main(String[] args) {
        double result = run(100_000_000);

        if (result == 0) {
            throw new RuntimeException();
        }
    }
}
