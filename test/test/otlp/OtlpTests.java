/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.otlp;

import java.io.ByteArrayOutputStream;
import java.nio.file.Path;
import java.nio.file.Files;
import java.util.*;
import java.util.stream.Collectors;

import one.convert.JfrToOtlp;
import one.convert.Arguments;
import one.jfr.JfrReader;
import one.profiler.test.*;

import io.opentelemetry.proto.common.v1.AnyValue;
import io.opentelemetry.proto.profiles.v1development.*;

public class OtlpTests {
    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,event=itimer,file=%f.pb")
    public void sampleType(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);
        checkSampleTypes(profilesData);
    }

    private static void checkSampleTypes(ProfilesData profilesData) {
        ValueType sampleType0 = getProfile(profilesData, 0).getSampleType();
        assertString(profilesData.getDictionary().getStringTable(sampleType0.getTypeStrindex()), "itimer");
        assertString(profilesData.getDictionary().getStringTable(sampleType0.getUnitStrindex()), "count");

        ValueType sampleType1 = getProfile(profilesData, 1).getSampleType();
        assertString(profilesData.getDictionary().getStringTable(sampleType1.getTypeStrindex()), "itimer");
        assertString(profilesData.getDictionary().getStringTable(sampleType1.getUnitStrindex()), "ns");
    }

    private static void assertString(String actual, String expected) {
        assert expected.equals(actual) : actual;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,threads,file=%f.pb")
    public void threadName(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        // counter
        checkThreadNames(getProfile(profilesData, 0), profilesData.getDictionary());
        // total
        checkThreadNames(getProfile(profilesData, 1), profilesData.getDictionary());
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,jfr,file=%f")
    public void threadNameFromJfr(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;

        ProfilesData profilesData = profilesDataFromJfr(p.getFilePath("%f"), new Arguments("--cpu", "--output", "otlp"));

        // counter
        checkThreadNames(getProfile(profilesData, 0), profilesData.getDictionary());
        // total
        checkThreadNames(getProfile(profilesData, 1), profilesData.getDictionary());
    }

    private static void checkThreadNames(Profile profile, ProfilesDictionary dictionary) {
        Set<String> threadNames = new HashSet<>();
        for (Sample sample : profile.getSamplesList()) {
            Optional<AnyValue> threadName = getAttribute(sample, dictionary, "thread.name");
            if (!threadName.isPresent()) continue;
            threadNames.add(threadName.get().getStringValue());
        }
        assert threadNames.stream().anyMatch(name -> name.contains("CpuBurnerWorker")) : "CpuBurner thread not found: " + threadNames;
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,otlp,file=%f.pb")
    public void samples(TestProcess p) throws Exception {
        ProfilesData profilesData = waitAndGetProfilesData(p);

        // counter
        checkSamples(getProfile(profilesData, 0), profilesData.getDictionary());
        // total
        checkSamples(getProfile(profilesData, 1), profilesData.getDictionary());
    }

    @Test(mainClass = CpuBurner.class, agentArgs = "start,jfr,file=%f")
    public void samplesFromJfr(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;

        ProfilesData profilesData = profilesDataFromJfr(p.getFilePath("%f"), new Arguments("--cpu", "--output", "otlp"));

        // counter
        checkSamples(getProfile(profilesData, 0), profilesData.getDictionary());
        // total
        checkSamples(getProfile(profilesData, 1), profilesData.getDictionary());
    }

    private static void checkSamples(Profile profile, ProfilesDictionary dictionary) {
        Output collapsed = toCollapsed(profile, dictionary);
        assert collapsed.containsExact("test/otlp/CpuBurner.lambda$main$0;test/otlp/CpuBurner.burn") : collapsed;
    }

    @Test(mainClass = OtlpTemporalityTest.class)
    public void profileTime(TestProcess p) throws Exception {
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

    private static ProfilesData profilesDataFromJfr(String jfrPath, Arguments args) throws Exception {
        JfrToOtlp converter;
        try (JfrReader jfr = new JfrReader(jfrPath)) {
            converter = new JfrToOtlp(jfr, args);
            converter.convert();
        }
        ByteArrayOutputStream os = new ByteArrayOutputStream();
        converter.dump(os);
        return ProfilesData.parseFrom(os.toByteArray());
    }

    private static Output toCollapsed(Profile profile, ProfilesDictionary dictionary) {
        return toCollapsed(profile, dictionary, 0);
    }

    private static Output toCollapsed(Profile profile, ProfilesDictionary dictionary, int valueIdx) {
        Map<String, Long> stackTracesCount = new HashMap<>();
        for (Sample sample : profile.getSamplesList()) {
            List<Integer> locations = dictionary.getStackTable(sample.getStackIndex()).getLocationIndicesList();
            StringBuilder stackTrace = new StringBuilder();
            for (int i = locations.size() - 1; i > 0; --i) {
                stackTrace.append(getFrameName(locations.get(i), dictionary)).append(';');
            }
            stackTrace.append(getFrameName(locations.get(locations.size() - 1), dictionary));

            stackTracesCount.compute(stackTrace.toString(), (key, oldValue) -> sample.getValues(valueIdx) + (oldValue == null ? 0 : oldValue));
        }
        List<String> lines = stackTracesCount.entrySet().stream().map(entry -> String.format("%s %d", entry.getKey(), entry.getValue())).collect(Collectors.toList());
        return new Output(lines.toArray(new String[0]));
    }

    private static String getFrameName(int locationIndex, ProfilesDictionary dictionary) {
        Location location = dictionary.getLocationTable(locationIndex);
        Line line = location.getLines(location.getLinesList().size() - 1);
        Function function = dictionary.getFunctionTable(line.getFunctionIndex());
        return dictionary.getStringTable(function.getNameStrindex());
    }

    private static Profile getProfile(ProfilesData profilesData, int index) {
        assert profilesData.getResourceProfilesList().size() == 1;

        ResourceProfiles resourceProfiles = profilesData.getResourceProfiles(0);
        assert resourceProfiles.getScopeProfilesList().size() == 1;

        ScopeProfiles scopeProfiles = resourceProfiles.getScopeProfiles(0);
        return scopeProfiles.getProfiles(index);
    }

    private static Optional<AnyValue> getAttribute(Sample sample, ProfilesDictionary dictionary, String name) {
        // Find the string table index for 'name'
        int keyStrindex = 0;
        for (; keyStrindex < dictionary.getStringTableList().size(); ++keyStrindex) {
            if (dictionary.getStringTable(keyStrindex).equals(name)) break;
        }
        if (keyStrindex == dictionary.getStringTableList().size()) {
            return Optional.empty();
        }

        for (int index : sample.getAttributeIndicesList()) {
            KeyValueAndUnit kv = dictionary.getAttributeTable(index);
            if (keyStrindex == kv.getKeyStrindex()) {
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
