/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

public class ExceptionThrow {
    static final String MESSAGE = "jfrsync-event-setting";

    private static void throwException() {
        try {
            throw new Exception(MESSAGE);
        } catch (Exception ignored) {
        }
    }

    public static void main(String[] args) throws Exception {
        long startTime = System.currentTimeMillis();
        while (System.currentTimeMillis() - startTime < 2000) {
            throwException();
            Thread.sleep(1);
        }
    }
}
