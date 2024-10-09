/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import one.convert.Arguments;
import one.convert.FlameGraph;
import one.convert.JfrToFlame;
import one.jfr.JfrReader;

import java.io.*;
import java.util.Arrays;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import java.util.stream.Stream;

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

    public Output convertFlameToCollapsed() throws IOException {
        FlameGraph fg = new FlameGraph(new Arguments("-o", "collapsed"));
        try (StringReader in = new StringReader(toString())) {
            fg.parseHtml(in);
        }

        try (ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
             PrintStream out = new PrintStream(outputStream)) {
            fg.dump(out);
            return new Output(outputStream.toString("UTF-8").split(System.lineSeparator()));
        }
    }

    public static Output convertJfrToCollapsed(String input, String... args) throws IOException {
        JfrToFlame converter;
        try (JfrReader jfr = new JfrReader(input)) {
            Arguments arguments = new Arguments(args);
            arguments.output = "collapsed";
            converter = new JfrToFlame(jfr, arguments);
            converter.convert();
        }

        try (ByteArrayOutputStream outputStream = new ByteArrayOutputStream()) {
            converter.dump(outputStream);
            return new Output(outputStream.toString("UTF-8").split(System.lineSeparator()));
        }
    }

    public double ratio(String regex1, String regex2) {
        long matched1 = samples(regex1);
        long matched2 = samples(regex2);

        return (double) matched1 / (matched1 + matched2);
    }

    public Output filter(String regex) {
        return new Output(stream(regex).toArray(String[]::new));
    }

    private static long extractSamples(String s) {
        return Long.parseLong(s.substring(s.lastIndexOf(' ') + 1));
    }
}
