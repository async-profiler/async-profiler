/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

import java.io.BufferedOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;

public class CollapsedStacks extends FlameGraph {
    private final StringBuilder sb = new StringBuilder();
    private final PrintStream out;

    public CollapsedStacks(Arguments args) throws IOException {
        super(args);
        this.out = args.output == null ? System.out : new PrintStream(
                new BufferedOutputStream(new FileOutputStream(args.output), 32768), false, "UTF-8");
    }

    @Override
    public void addSample(String[] trace, long ticks) {
        for (String s : trace) {
            sb.append(s).append(';');
        }
        if (sb.length() > 0) sb.setCharAt(sb.length() - 1, ' ');
        sb.append(ticks);

        out.println(sb.toString());
        sb.setLength(0);
    }

    @Override
    public void dump() {
        if (out != System.out) {
            out.close();
        }
    }
}
