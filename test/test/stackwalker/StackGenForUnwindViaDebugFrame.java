/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.stackwalker;

public class StackGenForUnwindViaDebugFrame {
    public static native double startWork();

    static {
        System.loadLibrary("dwarfdebugframe");
    }

    public static void main(String[] args) {
        startWork();
    }
}
