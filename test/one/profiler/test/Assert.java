/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.util.logging.Level;
import java.util.logging.Logger;

public class Assert {
    private static final Logger log = Logger.getLogger(Runner.class.getName());

    public static void isGreater(double value, double threshold) {
        isGreater(value, threshold, null);
    }

    public static void isGreater(double value, double threshold, String message) {
        boolean asserted = value <= threshold;
        log.log(Level.FINE, "isGreater (asserted: " + asserted + ") " + (message == null ? "" : message) + ": " + value + " > " + threshold);

        if (asserted) {
            throw new AssertionError(
                    "Expected " + value + " > " + threshold + (message != null ? (": " + message) : ""));
        }
    }

    public static void isLess(double value, double threshold) {
        isLess(value, threshold, null);
    }

    public static void isLess(double value, double threshold, String message) {
        boolean asserted = value >= threshold;
        log.log(Level.FINE, "isLess (asserted: " + asserted + ")" + (message == null ? "" : message) + ": " + value + " < " + threshold);

        if (asserted) {
            throw new AssertionError(
                    "Expected " + value + " < " + threshold + (message != null ? (": " + message) : ""));
        }
    }
}
