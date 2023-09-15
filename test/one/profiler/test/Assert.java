/*
 * Copyright 2021 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package one.profiler.test;

import java.util.Arrays;
import java.util.regex.Pattern;
import java.util.stream.Stream;

public class Assert {

    public static void contains(Output out, String regex) throws AssertionError {
        if (!out.contains(regex)) {
            throw new AssertionError("Expected: " + regex + "\n Received out may be available in build/test/logs.");
        }
    }

    public static void notContains(Output out, String regex) throws AssertionError {
        if (out.contains(regex)) {
            throw new AssertionError("Expected not: " + regex + "\n Received out may be available in build/test/logs.");
        }
    }

    public static void ratioGreater(Output out, String regex, double threshold) {
        double num = out.ratio(regex);
        if (num < threshold) {
            throw new AssertionError("Expected " + regex + " ratio > " + threshold + "\ngot: " + num + "\n Received out may be available in build/test/logs.");
        }
    }

    public static void ratioLess(Output out, String regex, double threshold) {
        double num = out.ratio(regex);
        if (num > threshold) {
            throw new AssertionError("Expected " + regex + " ratio < " + threshold + "\ngot: " + num + "\n Received out may be available in build/test/logs.");
        }
    }
}
