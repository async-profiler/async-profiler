/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.api;

import java.util.HashSet;
import one.profiler.AsyncProfiler;

public class MetricsTest {

    static volatile int sink = 0;

    static void doStuff() {
        sink++;
    }

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        profiler.execute("start,trace=test.api.MetricsTest.doStuff,features=stats,jfr,file=/dev/null");
        
        doStuff();
        doStuff();
        doStuff();

        String metrics = profiler.execute("metrics");
        for (String line : metrics.split("\n")) {
            String[] pair = line.split(" ");
            assert pair.length == 2 : line;
            if (pair[1].startsWith("0")) {
                assert "sample_failures_total".equals(pair[0]) || "calltracestorage_overflows_total".equals(pair[0]) : line;
            }

            if (pair[0].equals("total_samples")) {
                assert pair[1].equals("3") : line;
            }
        }

        // Should be found since we used features=stats
        assert metrics.contains("stackwalk_ns_total") : metrics;
    }
}
