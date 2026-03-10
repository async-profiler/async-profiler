/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfrconverter;

import one.convert.*;
import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.Event;
import one.jfr.event.EventCollector;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import test.otlp.CpuBurner;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Arrays;

// Simple smoke tests for JFR converter. The output is not inspected for errors,
// we only verify that the conversion completes successfully.
public class JfrconverterTests {

    @Test(mainClass = CpuBurner.class, agentArgs = "start,jfr,all,file=%f")
    public void heatmapConversion(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;
        JfrToHeatmap.convert(p.getFilePath("%f"), "/dev/null", new Arguments("--alloc"));
        JfrToHeatmap.convert(p.getFilePath("%f"), "/dev/null", new Arguments("--cpu"));
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,jfr,all,file=%f")
    public void flamegraphConversion(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;
        JfrToFlame.convert(p.getFilePath("%f"), "/dev/null", new Arguments());
        JfrToFlame.convert(p.getFilePath("%f"), "/dev/null", new Arguments("--alloc"));
    }

    @Test(mainClass = Tracer.class, agentArgs = "start,jfr,wall,trace=test.jfrconverter.Tracer.traceMethod,file=%f", runIsolated = true)
    public void latencyFilter(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;

        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            boolean[] found = new boolean[4];
            long minLatency = Tracer.TRACE_DURATION_MS - 10; // just to be sure
            JfrConverter converter = new JfrConverter(jfr, new Arguments("--wall", "--latency", minLatency + "")) {
                protected void convertChunk() {
                    collector.forEach(new EventCollector.Visitor() {
                        public void visit(Event event, long samples, long value) {
                            found[0] = true;

                            StackTrace stackTrace = jfr.stackTraces.get(event.stackTraceId);
                            if (stackTrace == null) return;

                            long[] methods = stackTrace.methods;
                            byte[] types = stackTrace.types;
                            for (int i = methods.length; --i >= 0; ) {
                                String methodName = getMethodName(methods[i], types[i]);
                                if (!methodName.startsWith("test/jfrconverter/Tracer.showcase")) continue;

                                int idx = Integer.parseInt(methodName.charAt(methodName.length() - 1) + "");
                                found[idx] = true;
                                break;
                            }
                        }
                    });
                }
            };
            converter.convert();

            assert found[0] : "No events found!";
            assert found[1];
            assert found[2];
            assert !found[3];
        }
    }

    @Test(mainClass = Main.class, args = "--diff test/test/jfrconverter/sample1.collapsed test/test/jfrconverter/sample2.collapsed %diff.collapsed")
    public void diffCollapsed(TestProcess p) throws Exception {
        Output out = p.waitForExit("%diff");
        assert out.containsExact("BusyClient.run_[j] 4 1");
        assert out.containsExact("BusyClient.run_[j];InputStream.read_[j];Socket$SocketInputStream.read_[j] 2 2");
        assert out.containsExact("ByteBuffer.get_[i];ByteBuffer.getArray_[i] 0 1");
        assert out.samples("ByteBuffer.get") == 2;
    }

    @Test(mainClass = Main.class, args = "--diff test/test/jfrconverter/sample1.collapsed test/test/jfrconverter/sample2.collapsed %diff.html")
    public void diffHtml(TestProcess p) throws Exception {
        Output out = p.waitForExit("%diff");
        assert out.containsExact("d=-3");
        assert out.containsExact("d=0");
        assert out.containsExact("d=U");

        // It should be possible to reconstruct original FlameGraph from the differential one
        byte[] original = buildFlameGraph("test/test/jfrconverter/sample2.collapsed");
        byte[] reconstructed = buildFlameGraph(p.getFilePath("%diff"));
        assert Arrays.equals(original, reconstructed);
    }

    private static byte[] buildFlameGraph(String input) throws IOException {
        FlameGraph fg = FlameGraph.parse(input, new Arguments());
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        fg.dump(baos);
        return baos.toByteArray();
    }
}
