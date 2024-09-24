/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.parser;

import jdk.jfr.consumer.RecordingFile;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

import java.nio.file.Paths;

/**
 * Test class to add tests validating JDK parser APIs for Async Profiler generated outputs.
 */
public class ParserTests {

    /**
     * Test to validate JDK APIs to parse AsyncProfiler JFR output
     *
     * @param p The test process to profile with.
     * @throws Exception Any exception thrown during profiling JFR output parsing.
     */
    @Test(mainClass = Cpu.class)
    public void cpu(TestProcess p) throws Exception {
        p.profile("-d 3 -e cpu -f %f.jfr");
        StringBuilder builder = new StringBuilder();
        try (RecordingFile recordingFile = new RecordingFile(Paths.get(p.getFile("%f").getAbsolutePath()))) {
            while (recordingFile.hasMoreEvents()) {
                builder.append(recordingFile.readEvent());
            }
        }

        assert builder.toString().contains("test.parser.Cpu.method1() line: 17");
    }
}
