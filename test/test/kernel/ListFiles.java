/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.kernel;

import java.io.File;

public class ListFiles {
    private static volatile int value;

    private static void listFiles() {
        for (String s : new File("/tmp").list()) {
            value += s.hashCode();
        }
    }

    public static void main(String[] args) {
        while (true) {
            listFiles();
        }
    }
}
