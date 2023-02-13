/*
 * Copyright 2022 Andrei Pangin
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
