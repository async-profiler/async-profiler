/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Comparator;
import java.util.StringTokenizer;
import java.util.regex.Pattern;

import static one.convert.Frame.*;
import static one.convert.ResourceProcessor.*;

public enum FlameGraphType {
    Default,
    Lock,
    Alloc;

    @Override
    public String toString() {
        switch (this) {
            case Lock:
                return "lock";
            case Alloc:
                return "alloc";
            default:
                return "default";
        }
    }
}
