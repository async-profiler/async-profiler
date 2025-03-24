/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.util.regex.Pattern;

public abstract class Filter {
    public static Pattern from(String s) {
        if (s.startsWith("*") && s.endsWith("*")) {
            // contains.
            return Pattern.compile(".*" + Pattern.quote(s.substring(1, s.length() - 1)) + ".*");
        }
        if (s.startsWith("*")) {
            // ends with.
            return Pattern.compile(".*" + Pattern.quote(s.substring(1)) + "$");
        }
        if (s.endsWith("*")) {
            // starts with.
            return Pattern.compile("^" + Pattern.quote(s.substring(0, s.length() - 1)) + ".*");
        }

        // equals
        return Pattern.compile("^" + Pattern.quote(s) + "$");
    }
}
