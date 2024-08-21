/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test.convert.impl;

import one.convert.Arguments;
import one.convert.FlameGraph;
import one.profiler.test.convert.OutputConverter;

import java.io.*;
import java.nio.charset.StandardCharsets;

/*
Output converter implementation for flamegraph to collapsed conversion.
 */
public class FlameToCollapsedConverter implements OutputConverter<InputStream, String> {

    @Override
    public String convert(InputStream inputStream, Arguments args) throws IOException {
        FlameGraph fg = new FlameGraph(args);
        try (InputStreamReader in = new InputStreamReader(inputStream, StandardCharsets.UTF_8)) {
            fg.parseHtml(in);
        }

        try (ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
             PrintStream out = new PrintStream(outputStream)) {
            fg.dump(out);
            return outputStream.toString();
        }
    }
}
