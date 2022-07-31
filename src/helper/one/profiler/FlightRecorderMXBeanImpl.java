/*
 * Copyright 2022 Andrei Pangin
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

package one.profiler;

import jdk.management.jfr.ConfigurationInfo;
import jdk.management.jfr.EventTypeInfo;
import jdk.management.jfr.FlightRecorderMXBean;
import jdk.management.jfr.RecordingInfo;

import javax.management.InstanceNotFoundException;
import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeType;
import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.lang.management.ManagementFactory;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class FlightRecorderMXBeanImpl implements FlightRecorderMXBean, CompositeData {
    private final Map<String, Object> cd = new HashMap<>();

    private boolean oldver;
    private long id;
    private String fileName;
    private long duration;
    private long startTime;
    private long stopTime;
    private RandomAccessFile raf;
    private Map<String, String> options = Collections.emptyMap();
    private Map<String, String> settings = Collections.emptyMap();

    private FlightRecorderMXBeanImpl() throws JMException {
        try {
            RecordingInfo.class.getDeclaredField("toDisk");
        } catch (NoSuchFieldException e) {
            oldver = true;
        }

        ObjectName name = getObjectName();
        try {
            ManagementFactory.getPlatformMBeanServer().unregisterMBean(name);
        } catch (InstanceNotFoundException e) {
            // ignore
        }
        ManagementFactory.getPlatformMBeanServer().registerMBean(this, name);
    }

    @Override
    public long newRecording() {
        return ++id;
    }

    @Override
    public long takeSnapshot() {
        return ++id;
    }

    @Override
    public long cloneRecording(long recordingId, boolean stop) {
        return ++id;
    }

    @Override
    public synchronized void startRecording(long recordingId) {
        try {
            File f = File.createTempFile("asprof" + id + "-", ".jfr");
            fileName = f.getAbsolutePath();
            f.deleteOnExit();

            duration = parseDuration(options.get("duration"));

            StringBuilder sb = new StringBuilder("start,jfr,file=").append(fileName);
            for (Map.Entry<String, String> e : settings.entrySet()) {
                if (e.getKey().startsWith("settings.AsyncProfiler#") && e.getValue() != null && !e.getValue().isEmpty()) {
                    sb.append(',').append(e.getKey().substring(23)).append('=').append(e.getValue());
                }
            }

            execute0(sb.toString());

            startTime = System.currentTimeMillis();
            stopTime = 0;
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    private long parseDuration(String s) {
        if (s == null || s.isEmpty()) {
            return 0;
        }

        long value = 0;
        for (int i = 0, len = s.length(); i < len; i++) {
            char c = s.charAt(i);
            if (c >= '0' && c <= '9') {
                value = value * 10 + (c - '0');
            } else if (c > ' ') {
                switch (Character.toLowerCase(c)) {
                    case 's':
                        return value;
                    case 'm':
                        return value * 60;
                    case 'h':
                        return value * 3600;
                    case 'd':
                        return value * 86400;
                }
                throw new IllegalArgumentException("Not a valid duration: " + s);
            }
        }
        return value;
    }

    @Override
    public synchronized boolean stopRecording(long recordingId) {
        try {
            execute0("stop");
            stopTime = System.currentTimeMillis();
            return true;
        } catch (IOException e) {
            return false;
        }
    }

    @Override
    public void closeRecording(long recordingId) {
    }

    @Override
    public synchronized long openStream(long recordingId, Map<String, String> streamOptions) throws IOException {
        raf = new RandomAccessFile(fileName, "r");
        return recordingId;
    }

    @Override
    public synchronized void closeStream(long streamId) throws IOException {
        if (raf != null) {
            try {
                raf.close();
            } finally {
                raf = null;
            }
        }
    }

    @Override
    public synchronized byte[] readStream(long streamId) throws IOException {
        if (raf == null) {
            throw new IOException("Stream closed");
        }

        byte[] buf = new byte[65536];
        int bytes = raf.read(buf);
        return bytes <= 0 ? null : bytes == buf.length ? buf : Arrays.copyOf(buf, bytes);
    }

    @Override
    public Map<String, String> getRecordingOptions(long recordingId) {
        return options;
    }

    @Override
    public Map<String, String> getRecordingSettings(long recordingId) {
        return settings;
    }

    @Override
    public void setConfiguration(long recordingId, String contents) {
    }

    @Override
    public void setPredefinedConfiguration(long recordingId, String configurationName) {
    }

    @Override
    public void setRecordingSettings(long recordingId, Map<String, String> settings) {
        this.settings = settings;
    }

    @Override
    public void setRecordingOptions(long recordingId, Map<String, String> options) {
        this.options = options;
    }

    @Override
    public synchronized List<RecordingInfo> getRecordings() {
        if (id == 0) {
            return Collections.emptyList();
        }
        if (stopTime == 0 && duration > 0 && System.currentTimeMillis() >= startTime + duration * 1000) {
            stopRecording(id);
        }
        return Collections.singletonList(newRecordingInfo());
    }

    private RecordingInfo newRecordingInfo() {
        cd.clear();
        cd.put("id", oldver ? (Object) (int) id : (Object) id);
        cd.put("name", options.get("name"));
        cd.put("state", stopTime > 0 ? "STOPPED" : "RUNNING");
        cd.put("dumpOnExit", true);
        cd.put("size", new File(fileName).length());
        cd.put("disk", true);
        cd.put("maxAge", 0L);
        cd.put("maxSize", 0L);
        cd.put("startTime", startTime);
        cd.put("stopTime", stopTime);
        cd.put("destination", fileName);
        cd.put("duration", duration);
        return RecordingInfo.from(this);
    }

    @Override
    public List<ConfigurationInfo> getConfigurations() {
        return Collections.singletonList(newConfigurationInfo("async-profiler"));
    }

    private ConfigurationInfo newConfigurationInfo(String name) {
        cd.clear();
        cd.put("name", name);
        cd.put("label", name);
        cd.put("provider", "async-profiler");
        cd.put("contents", "<configuration version=\"2.0\" label=\"" + name + "\" provider=\"async-profiler\">\n" +
                "  <event name=\"settings.AsyncProfiler\">\n" +
                "    <setting name=\"event\">cpu</setting>\n" +
                "    <setting name=\"interval\">10ms</setting>\n" +
                "    <setting name=\"alloc\"></setting>\n" +
                "    <setting name=\"lock\"></setting>\n" +
                "    <setting name=\"cstack\"></setting>\n" +
                "    <setting name=\"alluser\"></setting>\n" +
                "  </event>\n" +
                "</configuration>\n");
        return ConfigurationInfo.from(this);
    }

    @Override
    public List<EventTypeInfo> getEventTypes() {
        return Collections.emptyList();
    }

    @Override
    public void copyTo(long recordingId, String outputFile) throws IOException {
        Files.copy(Paths.get(fileName), Paths.get(outputFile), StandardCopyOption.REPLACE_EXISTING);
    }

    @Override
    public ObjectName getObjectName() {
        try {
            return new ObjectName("jdk.management.jfr:type=FlightRecorder");
        } catch (MalformedObjectNameException e) {
            throw new AssertionError(e);
        }
    }

    @Override
    public CompositeType getCompositeType() {
        throw new UnsupportedOperationException();
    }

    @Override
    public Object get(String key) {
        return cd.get(key);
    }

    @Override
    public Object[] getAll(String[] keys) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean containsKey(String key) {
        return cd.containsKey(key);
    }

    @Override
    public boolean containsValue(Object value) {
        return cd.containsValue(value);
    }

    @Override
    public Collection<?> values() {
        return cd.values();
    }

    private native String execute0(String command) throws IllegalArgumentException, IllegalStateException, IOException;
}
