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

import one.jfr.ClassRef;
import one.jfr.Frame;
import one.jfr.JfrReader;
import one.jfr.MethodRef;
import one.jfr.StackTrace;

import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

/**
 * Converts .jfr output produced by async-profiler to HTML Flame Graph.
 */
public class jfr2flame {

    private static final int FRAME_KERNEL = 6;

    private final JfrReader jfr;
    private final Map<Long, String> methodNames = new HashMap<>();

    public jfr2flame(JfrReader jfr) {
        this.jfr = jfr;
    }

    public void convert(final FlameGraph fg) {
        for (StackTrace stackTrace : jfr.stackTraces.values()) {
            Frame[] frames = stackTrace.frames;
            String[] trace = new String[frames.length];
            for (int i = 0; i < frames.length; i++) {
                trace[trace.length - 1 - i] = getMethodName(frames[i].method, frames[i].type);
            }
            fg.addSample(trace, stackTrace.samples);
        }
    }

    private String getMethodName(long methodId, int type) {
        String result = methodNames.get(methodId);
        if (result != null) {
            return result;
        }

        MethodRef method = jfr.methods.get(methodId);
        ClassRef cls = jfr.classes.get(method.cls);
        byte[] className = jfr.symbols.get(cls.name);
        byte[] methodName = jfr.symbols.get(method.name);

        if (className == null || className.length == 0) {
            String methodStr = new String(methodName, StandardCharsets.UTF_8);
            result = type == FRAME_KERNEL ? methodStr + "_[k]" : methodStr;
        } else {
            String classStr = new String(className, StandardCharsets.UTF_8);
            String methodStr = new String(methodName, StandardCharsets.UTF_8);
            result = classStr + '.' + methodStr + "_[j]";
        }

        methodNames.put(methodId, result);
        return result;
    }

    public static void main(String[] args) throws Exception {
        FlameGraph fg = new FlameGraph(args);
        if (fg.input == null) {
            System.out.println("Usage: java " + jfr2flame.class.getName() + " [options] input.jfr [output.html]");
            System.exit(1);
        }

        try (JfrReader jfr = new JfrReader(fg.input)) {
            new jfr2flame(jfr).convert(fg);
        }

        fg.dump();
    }
}
