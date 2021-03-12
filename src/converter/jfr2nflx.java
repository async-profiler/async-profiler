/*
 * Copyright 2020 Andrei Pangin
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
import one.jfr.Dictionary;
import one.jfr.JfrReader;
import one.jfr.MethodRef;
import one.jfr.Sample;
import one.jfr.StackTrace;
import one.proto.Proto;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Arrays;

/**
 * Converts .jfr output produced by async-profiler to nflxprofile format
 * as described in https://github.com/Netflix/nflxprofile/blob/master/nflxprofile.proto.
 * The result nflxprofile can be opened and analyzed with FlameScope.
 */
public class jfr2nflx {

    private static final String[] FRAME_TYPE = {"jit", "jit", "inlined", "user", "user", "kernel"};
    private static final byte[] NO_STACK = "[no_stack]".getBytes();

    private final JfrReader jfr;

    public jfr2nflx(JfrReader jfr) {
        this.jfr = jfr;
    }

    public void dump(OutputStream out) throws IOException {
        long startTime = System.nanoTime();

        int samples = jfr.samples.size();
        long durationTicks = samples == 0 ? 0 : jfr.samples.get(samples - 1).time - jfr.startTicks + 1;

        final Proto profile = new Proto(200000)
                .field(1, 0.0)
                .field(2, Math.max(jfr.durationNanos / 1e9, durationTicks / (double) jfr.ticksPerSec))
                .field(3, packSamples())
                .field(4, packDeltas())
                .field(6, "async-profiler")
                .field(8, new Proto(32).field(1, "has_node_stack").field(2, "true"))
                .field(8, new Proto(32).field(1, "has_samples_tid").field(2, "true"))
                .field(11, packTids());

        final Proto nodes = new Proto(10000);
        final Proto node = new Proto(10000);

        // Don't use lambda for faster startup
        jfr.stackTraces.forEach(new Dictionary.Visitor<StackTrace>() {
            @Override
            public void visit(long id, StackTrace stackTrace) {
                if (stackTrace.samples > 0) {
                    profile.field(5, nodes
                            .field(1, (int) id)
                            .field(2, packNode(node, stackTrace)));
                    nodes.reset();
                    node.reset();
                }
            }
        });

        out.write(profile.buffer(), 0, profile.size());

        long endTime = System.nanoTime();
        System.out.println("Wrote " + profile.size() + " bytes in " + (endTime - startTime) / 1e9 + " s");
    }

    private Proto packNode(Proto node, StackTrace stackTrace) {
        long[] methods = stackTrace.methods;
        byte[] types = stackTrace.types;
        int top = methods.length - 1;

        node.field(1, top >= 0 ? getMethodName(methods[top]) : NO_STACK);
        node.field(2, 1);
        node.field(4, top >= 0 ? FRAME_TYPE[types[top]] : "user");

        for (Proto frame = new Proto(100); --top >= 0; frame.reset()) {
            node.field(10, frame
                    .field(1, getMethodName(methods[top]))
                    .field(2, FRAME_TYPE[types[top]]));
        }

        return node;
    }

    private Proto packSamples() {
        Proto proto = new Proto(10000);
        for (Sample sample : jfr.samples) {
            proto.writeInt(sample.stackTraceId);
        }
        return proto;
    }

    private Proto packDeltas() {
        Proto proto = new Proto(10000);
        double ticksPerSec = jfr.ticksPerSec;
        long prevTime = jfr.startTicks;
        for (Sample sample : jfr.samples) {
            proto.writeDouble((sample.time - prevTime) / ticksPerSec);
            prevTime = sample.time;
        }
        return proto;
    }

    private Proto packTids() {
        Proto proto = new Proto(10000);
        for (Sample sample : jfr.samples) {
            proto.writeInt(sample.tid);
        }
        return proto;
    }

    private byte[] getMethodName(long methodId) {
        MethodRef method = jfr.methods.get(methodId);
        ClassRef cls = jfr.classes.get(method.cls);
        byte[] className = jfr.symbols.get(cls.name);
        byte[] methodName = jfr.symbols.get(method.name);

        if (className == null || className.length == 0) {
            return methodName;
        } else {
            byte[] fullName = Arrays.copyOf(className, className.length + 1 + methodName.length);
            fullName[className.length] = '.';
            System.arraycopy(methodName, 0, fullName, className.length + 1, methodName.length);
            return fullName;
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length < 2) {
            System.out.println("Usage: java " + jfr2nflx.class.getName() + " input.jfr output.nflx");
            System.exit(1);
        }

        File dst = new File(args[1]);
        if (dst.isDirectory()) {
            dst = new File(dst, new File(args[0]).getName().replace(".jfr", ".nflx"));
        }

        try (JfrReader jfr = new JfrReader(args[0]);
             FileOutputStream out = new FileOutputStream(dst)) {
            new jfr2nflx(jfr).dump(out);
        }
    }
}
