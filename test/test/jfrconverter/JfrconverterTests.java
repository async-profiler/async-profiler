/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfrconverter;

import test.otlp.CpuBurner;
import one.convert.*;
import one.jfr.JfrReader;
import one.jfr.event.Event;
import one.jfr.event.EventCollector;
import one.jfr.StackTrace;
import one.profiler.test.*;

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

    @Test(mainClass = Tracer.class, agentArgs = "start,jfr,wall,trace=test.jfrconverter.Tracer.traceMethod,file=%f")
    public void latencyFilter(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;

        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            boolean[] foundShowcase = new boolean[3];
            long minLatency = Tracer.TRACE_DURATION_MS - 10; // just to be sure
            JfrConverter converter = new JfrConverter(jfr, new Arguments("--wall", "--latency", minLatency + "")) {
                protected void convertChunk() {
                    collector.forEach(new EventCollector.Visitor() {
                        public void visit(Event event, long samples, long value) {
                            StackTrace stackTrace = jfr.stackTraces.get(event.stackTraceId);
                            if (stackTrace == null) return;

                            long[] methods = stackTrace.methods;
                            byte[] types = stackTrace.types;
                            for (int i = methods.length; --i >= 0; ) {
                                String methodName = getMethodName(methods[i], types[i]);
                                if (!methodName.startsWith("test/jfrconverter/Tracer.showcase")) continue;

                                int idx = Integer.parseInt(methodName.charAt(methodName.length() - 1) + "");
                                foundShowcase[idx - 1] = true;
                                break;
                            }
                        }
                    });
                }
            };
            converter.convert();

            assert foundShowcase[0];
            assert foundShowcase[1];
            assert !foundShowcase[2];
        }
    }
}
