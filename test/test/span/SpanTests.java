/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.span;

import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordingFile;
import one.profiler.test.Assert;
import one.profiler.test.Os;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

import java.time.Instant;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class SpanTests {

    private static int count(Map<String, Integer> tags, String tag) {
        return tags.getOrDefault(tag, 0);
    }

    @Test(mainClass = SpanApp.class, agentArgs = "start,event=cpu,interval=1ms,file=%f.jfr", os = Os.LINUX)
    public void spans(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        List<RecordedEvent> spans = new ArrayList<>();
        List<Instant> sampleTimes = new ArrayList<>();
        Map<String, Integer> tags = new HashMap<>();
        int nullTags = 0;
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                String name = event.getEventType().getName();
                if (name.equals("profiler.Span")) {
                    spans.add(event);
                    String tag = event.getString("tag");
                    if (tag == null) {
                        nullTags++;
                    } else {
                        tags.merge(tag, 1, Integer::sum);
                    }
                } else if (name.equals("jdk.ExecutionSample")) {
                    sampleTimes.add(event.getStartTime());
                }
            }
        }

        // Unconditional spans are always recorded, with or without an enclosed sample.
        assert tags.containsKey("busyRequest") : tags;
        assert tags.containsKey("idleRequest") : tags;
        assert nullTags >= 1 : "expected a span with a null tag";
        assert count(tags, "idleNormal") == SpanApp.IDLE_SPANS : tags;

        // Optional spans are kept only when they enclose a sample: the CPU-busy ones survive,
        // the idle (sleeping) ones are mostly skipped.
        assert tags.containsKey("busyOptional") : tags;
        assert count(tags, "idleOptional") < count(tags, "idleNormal") : tags;

        // The busy request lasted ~300ms; its duration must be recorded.
        RecordedEvent busyRequest = null;
        for (RecordedEvent span : spans) {
            if ("busyRequest".equals(span.getString("tag"))) {
                busyRequest = span;
            }
        }
        assert busyRequest != null;
        Assert.isGreater(busyRequest.getDuration().toMillis(), 200);

        // Timestamps align with profiling events: CPU samples fall inside the span.
        Instant from = busyRequest.getStartTime();
        Instant to = busyRequest.getEndTime();
        long enclosed = sampleTimes.stream().filter(t -> !t.isBefore(from) && !t.isAfter(to)).count();
        Assert.isGreater(enclosed, 0);
    }

    @Test(mainClass = SpanApiApp.class, args = "%f.jfr", os = Os.LINUX)
    public void api(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Set<String> tags = new HashSet<>();
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                if (event.getEventType().getName().equals("profiler.Span")) {
                    tags.add(event.getString("tag"));
                }
            }
        }

        // Only the span emitted while the session was active is recorded.
        assert tags.contains("duringSession") : tags;
        assert !tags.contains("beforeSession") : tags;
        assert !tags.contains("afterSession") : tags;
    }

    @Test(mainClass = SpanAttachApp.class, os = Os.LINUX)
    public void attach(TestProcess p) throws Exception {
        // async-profiler is attached after the application has already started using the Span API.
        p.profile("-e cpu -i 1ms -d 1 -f %f.jfr");

        Set<String> tags = new HashSet<>();
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                if (event.getEventType().getName().equals("profiler.Span")) {
                    tags.add(event.getString("tag"));
                }
            }
        }

        assert tags.contains("attachRequest") : tags;
    }
}
