/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfrconverter;

import one.convert.*;
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
        p.waitForExit("%f");
        assert p.exitCode() == 0;
        JfrToHeatmap.convert(p.getFilePath("%f"), "/dev/null", new Arguments("--alloc"));
        JfrToHeatmap.convert(p.getFilePath("%f"), "/dev/null", new Arguments("--cpu"));
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,jfr,all,file=%f")
    public void flamegraphConversion(TestProcess p) throws Exception {
        p.waitForExit("%f");
        assert p.exitCode() == 0;
        JfrToFlame.convert(p.getFilePath("%f"), "/dev/null", new Arguments());
        JfrToFlame.convert(p.getFilePath("%f"), "/dev/null", new Arguments("--alloc"));
    }

    @Test(mainClass = Tracer.class, agentArgs = "start,jfr,wall=20ms,trace=test.jfrconverter.Tracer.traceMethod,file=%f", runIsolated = true)
    public void latencyFilter(TestProcess p) throws Exception {
        p.waitForExit("%f");
        assert p.exitCode() == 0;

        long minLatency = Tracer.TRACE_DURATION_MS - 10;
        Output out = Output.convertJfrToCollapsed(p.getFilePath("%f"), "--wall", "--latency", minLatency + "");

        assert out.containsExact("Tracer.showcase1");
        assert out.containsExact("Tracer.showcase2");
        assert !out.containsExact("Tracer.showcase3");
    }

    @Test(mainClass = Tagger.class, agentArgs = "start,jfr,wall=20ms,file=%f")
    public void tagFilter(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        String file = p.getFilePath("%f");

        Output out1 = Output.convertJfrToCollapsed(file, "--tag", "showcase0");
        assert out1.containsExact("showcase0") && !out1.containsExact("showcase1") && !out1.containsExact("showcase2");

        Output out2 = Output.convertJfrToCollapsed(file, "--tag", "showcase.*");
        assert out2.stream().count() >= 3;
        assert out2.containsExact("showcase0") && out2.containsExact("showcase1") && out2.containsExact("showcase2");

        Output out3 = Output.convertJfrToCollapsed(file, "--tag", "missing");
        assert out3.toString().isEmpty();
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
