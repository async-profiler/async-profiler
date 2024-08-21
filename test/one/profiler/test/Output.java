/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import one.convert.Arguments;
import one.profiler.test.convert.impl.FlameToCollapsedConverter;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Locale;
import java.util.regex.Pattern;
import java.util.stream.Stream;

import static one.profiler.test.ProfileOutputType.COLLAPSED;
import static one.profiler.test.ProfileOutputType.FLAMEGRAPH;

public class Output {
    private final String[] lines;

    public Output(String[] lines) {
        this.lines = lines;
    }

    @Override
    public String toString() {
        return String.join("\n", lines);
    }

    public Stream<String> stream() {
        return Arrays.stream(lines);
    }

    public Stream<String> stream(String regex) {
        Pattern pattern = Pattern.compile(regex);
        return Arrays.stream(lines).filter(s -> pattern.matcher(s).find());
    }

    public boolean contains(String regex) {
        return stream(regex).findAny().isPresent();
    }

    public long samples(String regex) {
        return stream(regex).mapToLong(Output::extractSamples).sum();
    }

    public double ratio(String regex) {
        long total = 0;
        long matched = 0;
        Pattern pattern = Pattern.compile(regex);

        for (String s : lines) {
            long samples = extractSamples(s);
            total += samples;
            matched += pattern.matcher(s).find() ? samples : 0;
        }
        return (double) matched / total;
    }

    public String convert(ProfileOutputType fromOutputType, ProfileOutputType toOutputType, InputStream inputStream) {
        try {
            if (fromOutputType.equals(FLAMEGRAPH) && toOutputType.equals(COLLAPSED)) {
                return new FlameToCollapsedConverter().convert(
                        inputStream, new Arguments("-o", "collapsed"));
            } else {
                throw new RuntimeException("Conversion for the provided fromOutputType is not implemented!");
            }
        } catch (IOException e) {
            return "";
        }
    }

    private static long extractSamples(String s) {
        return Long.parseLong(s.substring(s.lastIndexOf(' ') + 1));
    }
}
