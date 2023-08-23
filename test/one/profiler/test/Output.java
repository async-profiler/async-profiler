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

    private static long extractSamples(String s) {
        return Long.parseLong(s.substring(s.lastIndexOf(' ') + 1));
    }
}
