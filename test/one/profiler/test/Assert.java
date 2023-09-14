/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

public class Assert {

    public static void contains(Output out, String regex) throws AssertionError {
        if (!out.contains(regex)) {
            throw new AssertionError("Expected: " + regex + "\nReceived out may be available in build/test/logs.");
        }
    }

    public static void notContains(Output out, String regex) throws AssertionError {
        if (out.contains(regex)) {
            throw new AssertionError("Expected not: " + regex + "\nReceived out may be available in build/test/logs.");
        }
    }

    public static void ratioGreater(Output out, String regex, double threshold) {
        double num = out.ratio(regex);
        if (num < threshold) {
            throw new AssertionError("Expected " + regex + " ratio > " + threshold + "\ngot: " + num + "\nReceived out may be available in build/test/logs.");
        }
    }

    public static void ratioLess(Output out, String regex, double threshold) {
        double num = out.ratio(regex);
        if (num > threshold) {
            throw new AssertionError("Expected " + regex + " ratio < " + threshold + "\ngot: " + num + "\nReceived out may be available in build/test/logs.");
        }
    }
}
