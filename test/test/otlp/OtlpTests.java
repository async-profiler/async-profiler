/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.otlp;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

import one.profiler.test.*;
import io.opentelemetry.proto.profiles.v1development.*;

public class OtlpTests {
    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,file=%f.pb")
    public void testOtlpReadable(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        assert getFirstProfile(profilesData) != null;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,event=itimer,file=%f.pb")
    public void testSampleTypeCpuProfiling(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        Profile profile = getFirstProfile(profilesData);
        assert profile.getSampleTypeList().size() == 2;

        assert "itimer".equals(getString(profilesData, profile.getPeriodType().getTypeStrindex()));
        assert "nanoseconds".equals(getString(profilesData, profile.getPeriodType().getUnitStrindex()));

        ValueType sampleType0 = profile.getSampleType(0);
        assert "samples".equals(getString(profilesData, sampleType0.getTypeStrindex()));
        assert "count".equals(getString(profilesData, sampleType0.getUnitStrindex()));
        assert sampleType0.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;

        ValueType sampleType1 = profile.getSampleType(1);
        assert "itimer".equals(getString(profilesData, sampleType1.getTypeStrindex()));
        assert "nanoseconds".equals(getString(profilesData, sampleType1.getUnitStrindex()));
        assert sampleType1.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,event=wall,file=%f.pb")
    public void testSampleTypeWallClockProfiling(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        Profile profile = getFirstProfile(profilesData);
        assert profile.getSampleTypeList().size() == 2;

        assert "wall".equals(getString(profilesData, profile.getPeriodType().getTypeStrindex()));
        assert "nanoseconds".equals(getString(profilesData, profile.getPeriodType().getUnitStrindex()));

        ValueType sampleType0 = profile.getSampleType(0);
        assert "samples".equals(getString(profilesData, sampleType0.getTypeStrindex()));
        assert "count".equals(getString(profilesData, sampleType0.getUnitStrindex()));
        assert sampleType0.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;

        ValueType sampleType1 = profile.getSampleType(1);
        assert "wall".equals(getString(profilesData, sampleType1.getTypeStrindex()));
        assert "nanoseconds".equals(getString(profilesData, sampleType1.getUnitStrindex()));
        assert sampleType1.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,nativemem,file=%f.pb")
    public void testSampleTypeNativemem(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        Profile profile = getFirstProfile(profilesData);
        assert profile.getSampleTypeList().size() == 2;

        assert "malloc".equals(getString(profilesData, profile.getPeriodType().getTypeStrindex()));
        assert "bytes".equals(getString(profilesData, profile.getPeriodType().getUnitStrindex()));

        ValueType sampleType0 = profile.getSampleType(0);
        assert "samples".equals(getString(profilesData, sampleType0.getTypeStrindex()));
        assert "count".equals(getString(profilesData, sampleType0.getUnitStrindex()));
        assert sampleType0.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;

        ValueType sampleType1 = profile.getSampleType(1);
        assert "malloc".equals(getString(profilesData, sampleType1.getTypeStrindex()));
        assert "bytes".equals(getString(profilesData, sampleType1.getUnitStrindex()));
        assert sampleType1.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,alloc,file=%f.pb")
    public void testSampleTypeAlloc(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        Profile profile = getFirstProfile(profilesData);
        assert profile.getSampleTypeList().size() == 2;

        assert "alloc".equals(getString(profilesData, profile.getPeriodType().getTypeStrindex()));
        assert "bytes".equals(getString(profilesData, profile.getPeriodType().getUnitStrindex()));

        ValueType sampleType0 = profile.getSampleType(0);
        assert "samples".equals(getString(profilesData, sampleType0.getTypeStrindex()));
        assert "count".equals(getString(profilesData, sampleType0.getUnitStrindex()));
        assert sampleType0.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;

        ValueType sampleType1 = profile.getSampleType(1);
        assert "alloc".equals(getString(profilesData, sampleType1.getTypeStrindex()));
        assert "bytes".equals(getString(profilesData, sampleType1.getUnitStrindex()));
        assert sampleType1.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,event=test.cpu.CpuBurner.main,file=%f.pb")
    public void testSampleTypeInstrument(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        Profile profile = getFirstProfile(profilesData);
        assert profile.getSampleTypeList().size() == 2;

        assert "test/cpu/CpuBurner".equals(getString(profilesData, profile.getPeriodType().getTypeStrindex()));
        assert "calls".equals(getString(profilesData, profile.getPeriodType().getUnitStrindex()));

        ValueType sampleType0 = profile.getSampleType(0);
        assert "samples".equals(getString(profilesData, sampleType0.getTypeStrindex()));
        assert "count".equals(getString(profilesData, sampleType0.getUnitStrindex()));
        assert sampleType0.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;

        ValueType sampleType1 = profile.getSampleType(1);
        assert "test/cpu/CpuBurner".equals(getString(profilesData, sampleType1.getTypeStrindex()));
        assert "calls".equals(getString(profilesData, sampleType1.getUnitStrindex()));
        assert sampleType1.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,file=%f.pb")
    public void testSamples(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        Profile profile = getFirstProfile(profilesData);
        ProfilesDictionary dictionary = profilesData.getDictionary();

        Output collapsed = toCollapsed(profile, dictionary);
        assert collapsed.contains("test/otlp/CpuBurner.main;test/otlp/CpuBurner.burn");
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

    private static void assertCloseTo(long value, long target, String message) {
        Assert.isGreaterOrEqual(value, target * 0.75, message);
        Assert.isLessOrEqual(value, target * 1.25, message);
    }

    private static String getString(ProfilesData profilesData, int index) {
        return profilesData.getDictionary().getStringTable(index);
    }
}
