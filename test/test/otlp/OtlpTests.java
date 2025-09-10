/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.otlp;

import java.nio.file.Files;
import java.util.*;
import java.util.stream.Collectors;

import io.opentelemetry.proto.common.v1.AnyValue;
import io.opentelemetry.proto.common.v1.KeyValue;
import one.profiler.test.*;
import io.opentelemetry.proto.profiles.v1development.*;

public class OtlpTests {
    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,file=%f.pb")
    public void readable(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        assert getFirstProfile(profilesData) != null;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,event=itimer,file=%f.pb")
    public void sampleType(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        Profile profile = getFirstProfile(profilesData);
        assert profile.getSampleTypeList().size() == 2;

        ValueType sampleType0 = profile.getSampleType(0);
        assert profilesData.getDictionary().getStringTable(sampleType0.getTypeStrindex()).equals("itimer");
        assert profilesData.getDictionary().getStringTable(sampleType0.getUnitStrindex()).equals("count");
        assert sampleType0.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;

        ValueType sampleType1 = profile.getSampleType(1);
        assert profilesData.getDictionary().getStringTable(sampleType1.getTypeStrindex()).equals("itimer");
        assert profilesData.getDictionary().getStringTable(sampleType1.getUnitStrindex()).equals("ns");
        assert sampleType1.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,threads,file=%f.pb")
    public void threadName(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        Profile profile = getFirstProfile(profilesData);
        assert profile.getSampleTypeList().size() == 2;

        Set<String> threadNames = new HashSet<>();
        for (Sample sample: profile.getSampleList()) {
            Optional<AnyValue> threadName = getAttribute(sample, profilesData.getDictionary(), "thread.name");
            if (!threadName.isPresent()) continue;
            threadNames.add(threadName.get().getStringValue());
        }
        assert threadNames.contains("CpuBurnerWorker") : "CpuBurner thread not found: " + threadNames;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,file=%f.pb")
    public void samples(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        Profile profile = getFirstProfile(profilesData);
        ProfilesDictionary dictionary = profilesData.getDictionary();

        Output collapsed = toCollapsed(profile, dictionary);
        assert collapsed.containsExact("test/otlp/CpuBurner.lambda$main$0;test/otlp/CpuBurner.burn") : collapsed;
    }

    @Test(mainClass = OtlpTemporalityTest.class, jvmArgs = "-Djava.library.path=build/lib")
    public void aggregationTemporality(TestProcess p) throws Exception {
        classpathCheck();

        p.waitForExit();
        assert p.exitCode() == 0;
    }

    private static ProfilesData waitAndGetProfilesData(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        byte[] profileBytes = Files.readAllBytes(p.getFile("%f").toPath());
        return ProfilesData.parseFrom(profileBytes);
    }

    private static Output toCollapsed(Profile profile, ProfilesDictionary dictionary) {
        return toCollapsed(profile, dictionary, 0);
    }

    private static Output toCollapsed(Profile profile, ProfilesDictionary dictionary, int valueIdx) {
        Map<String, Long> stackTracesCount = new HashMap<>();
        for (Sample sample : profile.getSampleList()) {
            StringBuilder stackTrace = new StringBuilder();
            for (int i = sample.getLocationsLength() - 1; i > 0; --i) {
                int locationIndex = profile.getLocationIndices(sample.getLocationsStartIndex() + i);
                stackTrace.append(getFrameName(locationIndex, dictionary)).append(';');
            }
            int locationIndex = profile.getLocationIndices(sample.getLocationsStartIndex());
            stackTrace.append(getFrameName(locationIndex, dictionary));

            stackTracesCount.compute(stackTrace.toString(), (key, oldValue) -> sample.getValue(valueIdx) + (oldValue == null ? 0 : oldValue));
        }
        List<String> lines = stackTracesCount.entrySet().stream().map(entry -> String.format("%s %d", entry.getKey(), entry.getValue())).collect(Collectors.toList());
        return new Output(lines.toArray(new String[0]));
    }

    private static String getFrameName(int locationIndex, ProfilesDictionary dictionary) {
        Location location = dictionary.getLocationTable(locationIndex);
        Line line = location.getLine(location.getLineList().size() - 1);
        Function function = dictionary.getFunctionTable(line.getFunctionIndex());
        return dictionary.getStringTable(function.getNameStrindex());
    }

    private static Profile getFirstProfile(ProfilesData profilesData) {
        assert profilesData.getResourceProfilesList().size() == 1;

        ResourceProfiles resourceProfiles = profilesData.getResourceProfiles(0);
        assert resourceProfiles.getScopeProfilesList().size() == 1;

        ScopeProfiles scopeProfiles = resourceProfiles.getScopeProfiles(0);
        assert scopeProfiles.getProfilesList().size() == 1;

        return scopeProfiles.getProfiles(0);
    }

    private static Optional<AnyValue> getAttribute(Sample sample, ProfilesDictionary dictionary, String name) {
        for (int index : sample.getAttributeIndicesList()) {
            KeyValue kv = dictionary.getAttributeTable(index);
            if (name.equals(kv.getKey())) {
                return Optional.of(kv.getValue());
            }
        }
        return Optional.empty();
    }

    // Simple check to make sure the classpath contains the required dependencies.
    // If not, it will throw a NoClassDefFoundError, which will be caught by the test runner.
    // It's useful to anticipate the same NoClassDefFoundError to happen in the child process,
    // where it's harder to detect.
    private static void classpathCheck() {
        ProfilesData.newBuilder();
    }
}
