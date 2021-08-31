/*
 * Copyright 2020 Andrei Pangin
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

package one.jfr;

import one.jfr.event.AllocationSample;
import one.jfr.event.ContendedLock;
import one.jfr.event.Event;
import one.jfr.event.ExecutionSample;

import java.io.Closeable;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Parses JFR output produced by async-profiler.
 */
public class JfrReader implements Closeable {
    private static final int BUFFER_SIZE = 2 * 1024 * 1024;
    private static final int CHUNK_HEADER_SIZE = 68;
    private static final int CHUNK_SIGNATURE = 0x464c5200;

    private final FileChannel ch;
    private ByteBuffer buf;
    private long filePosition;

    public final long startNanos;
    public final long durationNanos;
    public final long startTicks;
    public final long ticksPerSec;

    public final Dictionary<JfrClass> types = new Dictionary<>();
    public final Map<String, JfrClass> typesByName = new HashMap<>();
    public final Dictionary<String> threads = new Dictionary<>();
    public final Dictionary<ClassRef> classes = new Dictionary<>();
    public final Dictionary<byte[]> symbols = new Dictionary<>();
    public final Dictionary<MethodRef> methods = new Dictionary<>();
    public final Dictionary<StackTrace> stackTraces = new Dictionary<>();
    public final Map<Integer, String> frameTypes = new HashMap<>();
    public final Map<Integer, String> threadStates = new HashMap<>();

    private final int executionSample;
    private final int nativeMethodSample;
    private final int allocationInNewTLAB;
    private final int allocationOutsideTLAB;
    private final int monitorEnter;
    private final int threadPark;

    private final boolean hasPreviousOwner;
    private final boolean hasParkUntil;

    public JfrReader(String fileName) throws IOException {
        this.ch = FileChannel.open(Paths.get(fileName), StandardOpenOption.READ);
        this.buf = ByteBuffer.allocateDirect(BUFFER_SIZE);

        long[] times = {Long.MAX_VALUE, Long.MIN_VALUE, Long.MAX_VALUE, 0};

        for (long chunkStart = 0, fileSize = ch.size(); chunkStart < fileSize; ) {
            chunkStart += readChunk(chunkStart, times);
        }

        this.startNanos = times[0];
        this.durationNanos = times[1] - startNanos;
        this.startTicks = times[2];
        this.ticksPerSec = times[3];

        this.executionSample = getTypeId("jdk.ExecutionSample");
        this.nativeMethodSample = getTypeId("jdk.NativeMethodSample");
        this.allocationInNewTLAB = getTypeId("jdk.ObjectAllocationInNewTLAB");
        this.allocationOutsideTLAB = getTypeId("jdk.ObjectAllocationOutsideTLAB");
        this.monitorEnter = getTypeId("jdk.JavaMonitorEnter");
        this.threadPark = getTypeId("jdk.ThreadPark");

        this.hasPreviousOwner = hasField("jdk.JavaMonitorEnter", "previousOwner");
        this.hasParkUntil = hasField("jdk.ThreadPark", "until");

        seek(CHUNK_HEADER_SIZE);
    }

    @Override
    public void close() throws IOException {
        ch.close();
    }

    public List<Event> readAllEvents() throws IOException {
        return readAllEvents(null);
    }

    public <E extends Event> List<E> readAllEvents(Class<E> cls) throws IOException {
        ArrayList<E> events = new ArrayList<>();
        for (E event; (event = readEvent(cls)) != null; ) {
            events.add(event);
        }
        Collections.sort(events);
        return events;
    }

    public Event readEvent() throws IOException {
        return readEvent(null);
    }

    @SuppressWarnings("unchecked")
    public <E extends Event> E readEvent(Class<E> cls) throws IOException {
        while (ensureBytes(CHUNK_HEADER_SIZE)) {
            int pos = buf.position();
            int size = getVarint();
            int type = getVarint();

            if (type == 'L' && buf.getInt(pos) == CHUNK_SIGNATURE) {
                buf.position(pos + CHUNK_HEADER_SIZE);
                continue;
            }

            if (type == executionSample || type == nativeMethodSample) {
                if (cls == null || cls == ExecutionSample.class) return (E) readExecutionSample();
            } else if (type == allocationInNewTLAB) {
                if (cls == null || cls == AllocationSample.class) return (E) readAllocationSample(true);
            } else if (type == allocationOutsideTLAB) {
                if (cls == null || cls == AllocationSample.class) return (E) readAllocationSample(false);
            } else if (type == monitorEnter) {
                if (cls == null || cls == ContendedLock.class) return (E) readContendedLock(false, hasPreviousOwner);
            } else if (type == threadPark) {
                if (cls == null || cls == ContendedLock.class) return (E) readContendedLock(true, hasParkUntil);
            }

            if ((pos += size) <= buf.limit()) {
                buf.position(pos);
            } else {
                seek(filePosition + pos);
            }
        }
        return null;
    }

    private ExecutionSample readExecutionSample() {
        long time = getVarlong();
        int tid = getVarint();
        int stackTraceId = getVarint();
        int threadState = getVarint();
        return new ExecutionSample(time, tid, stackTraceId, threadState);
    }

    private AllocationSample readAllocationSample(boolean tlab) {
        long time = getVarlong();
        int tid = getVarint();
        int stackTraceId = getVarint();
        int classId = getVarint();
        long allocationSize = getVarlong();
        long tlabSize = tlab ? getVarlong() : 0;
        return new AllocationSample(time, tid, stackTraceId, classId, allocationSize, tlabSize);
    }

    private ContendedLock readContendedLock(boolean hasTimeout, boolean hasExtraField) {
        long time = getVarlong();
        long duration = getVarlong();
        int tid = getVarint();
        int stackTraceId = getVarint();
        int classId = getVarint();
        if (hasTimeout) getVarlong();
        if (hasExtraField) getVarlong();
        long address = getVarlong();
        return new ContendedLock(time, tid, stackTraceId, duration, classId);
    }

    private long readChunk(long chunkStart, long[] times) throws IOException {
        seek(chunkStart);
        ensureBytes(CHUNK_HEADER_SIZE);

        if (buf.getInt(0) != CHUNK_SIGNATURE) {
            throw new IOException("Not a valid JFR file");
        }

        int version = buf.getInt(4);
        if (version < 0x20000 || version > 0x2ffff) {
            throw new IOException("Unsupported JFR version: " + (version >>> 16) + "." + (version & 0xffff));
        }

        long chunkSize = buf.getLong(8);
        long cpOffset = buf.getLong(16);
        long metaOffset = buf.getLong(24);

        times[0] = Math.min(times[0], buf.getLong(32));
        times[1] = Math.max(times[1], buf.getLong(32) + buf.getLong(40));
        times[2] = Math.min(times[2], buf.getLong(48));
        times[3] = buf.getLong(56);

        readMeta(chunkStart + metaOffset);
        readConstantPool(chunkStart + cpOffset);

        return chunkSize;
    }

    private void readMeta(long metaOffset) throws IOException {
        seek(metaOffset);
        ensureBytes(5);

        ensureBytes(getVarint() - buf.position());
        getVarint();
        getVarlong();
        getVarlong();
        getVarlong();

        String[] strings = new String[getVarint()];
        for (int i = 0; i < strings.length; i++) {
            strings[i] = getString();
        }
        readElement(strings);
    }

    private Element readElement(String[] strings) {
        String name = strings[getVarint()];

        int attributeCount = getVarint();
        Map<String, String> attributes = new HashMap<>(attributeCount);
        for (int i = 0; i < attributeCount; i++) {
            attributes.put(strings[getVarint()], strings[getVarint()]);
        }

        Element e = createElement(name, attributes);
        int childCount = getVarint();
        for (int i = 0; i < childCount; i++) {
            e.addChild(readElement(strings));
        }
        return e;
    }

    private Element createElement(String name, Map<String, String> attributes) {
        switch (name) {
            case "class": {
                JfrClass type = new JfrClass(attributes);
                if (!attributes.containsKey("superType")) {
                    types.put(type.id, type);
                }
                typesByName.put(type.name, type);
                return type;
            }
            case "field":
                return new JfrField(attributes);
            default:
                return new Element();
        }
    }

    private void readConstantPool(long cpOffset) throws IOException {
        long delta;
        do {
            seek(cpOffset);
            ensureBytes(5);

            ensureBytes(getVarint() - buf.position());
            getVarint();
            getVarlong();
            getVarlong();
            delta = getVarlong();
            getVarint();

            int poolCount = getVarint();
            for (int i = 0; i < poolCount; i++) {
                int type = getVarint();
                readConstants(types.get(type));
            }
        } while (delta != 0 && (cpOffset += delta) > 0);
    }

    private void readConstants(JfrClass type) {
        switch (type.name) {
            case "jdk.types.ChunkHeader":
                buf.position(buf.position() + (CHUNK_HEADER_SIZE + 3));
                break;
            case "java.lang.Thread":
                readThreads(type.field("group") != null);
                break;
            case "java.lang.Class":
                readClasses(type.field("hidden") != null);
                break;
            case "jdk.types.Symbol":
                readSymbols();
                break;
            case "jdk.types.Method":
                readMethods();
                break;
            case "jdk.types.StackTrace":
                readStackTraces();
                break;
            case "jdk.types.FrameType":
                readMap(frameTypes);
                break;
            case "jdk.types.ThreadState":
                readMap(threadStates);
                break;
            default:
                readOtherConstants(type.fields);
        }
    }

    private void readThreads(boolean hasGroup) {
        int count = threads.preallocate(getVarint());
        for (int i = 0; i < count; i++) {
            long id = getVarlong();
            String osName = getString();
            int osThreadId = getVarint();
            String javaName = getString();
            long javaThreadId = getVarlong();
            if (hasGroup) getVarlong();
            threads.put(id, javaName != null ? javaName : osName);
        }
    }

    private void readClasses(boolean hasHidden) {
        int count = classes.preallocate(getVarint());
        for (int i = 0; i < count; i++) {
            long id = getVarlong();
            long loader = getVarlong();
            long name = getVarlong();
            long pkg = getVarlong();
            int modifiers = getVarint();
            if (hasHidden) getVarint();
            classes.put(id, new ClassRef(name));
        }
    }

    private void readMethods() {
        int count = methods.preallocate(getVarint());
        for (int i = 0; i < count; i++) {
            long id = getVarlong();
            long cls = getVarlong();
            long name = getVarlong();
            long sig = getVarlong();
            int modifiers = getVarint();
            int hidden = getVarint();
            methods.put(id, new MethodRef(cls, name, sig));
        }
    }

    private void readStackTraces() {
        int count = stackTraces.preallocate(getVarint());
        for (int i = 0; i < count; i++) {
            long id = getVarlong();
            int truncated = getVarint();
            StackTrace stackTrace = readStackTrace();
            stackTraces.put(id, stackTrace);
        }
    }

    private StackTrace readStackTrace() {
        int depth = getVarint();
        long[] methods = new long[depth];
        byte[] types = new byte[depth];
        for (int i = 0; i < depth; i++) {
            methods[i] = getVarlong();
            int line = getVarint();
            int bci = getVarint();
            types[i] = buf.get();
        }
        return new StackTrace(methods, types);
    }

    private void readSymbols() {
        int count = symbols.preallocate(getVarint());
        for (int i = 0; i < count; i++) {
            long id = getVarlong();
            if (buf.get() != 3) {
                throw new IllegalArgumentException("Invalid symbol encoding");
            }
            symbols.put(id, getBytes());
        }
    }

    private void readMap(Map<Integer, String> map) {
        int count = getVarint();
        for (int i = 0; i < count; i++) {
            map.put(getVarint(), getString());
        }
    }

    private void readOtherConstants(List<JfrField> fields) {
        int stringType = getTypeId("java.lang.String");

        boolean[] numeric = new boolean[fields.size()];
        for (int i = 0; i < numeric.length; i++) {
            JfrField f = fields.get(i);
            numeric[i] = f.constantPool || f.type != stringType;
        }

        int count = getVarint();
        for (int i = 0; i < count; i++) {
            getVarlong();
            readFields(numeric);
        }
    }

    private void readFields(boolean[] numeric) {
        for (boolean n : numeric) {
            if (n) {
                getVarlong();
            } else {
                getString();
            }
        }
    }

    private int getTypeId(String typeName) {
        JfrClass type = typesByName.get(typeName);
        return type != null ? type.id : -1;
    }

    private boolean hasField(String typeName, String fieldName) {
        JfrClass type = typesByName.get(typeName);
        return type != null && type.field(fieldName) != null;
    }

    private int getVarint() {
        int result = 0;
        for (int shift = 0; ; shift += 7) {
            byte b = buf.get();
            result |= (b & 0x7f) << shift;
            if (b >= 0) {
                return result;
            }
        }
    }

    private long getVarlong() {
        long result = 0;
        for (int shift = 0; shift < 56; shift += 7) {
            byte b = buf.get();
            result |= (b & 0x7fL) << shift;
            if (b >= 0) {
                return result;
            }
        }
        return result | (buf.get() & 0xffL) << 56;
    }

    private String getString() {
        switch (buf.get()) {
            case 0:
                return null;
            case 1:
                return "";
            case 3:
                return new String(getBytes(), StandardCharsets.UTF_8);
            case 4: {
                char[] chars = new char[getVarint()];
                for (int i = 0; i < chars.length; i++) {
                    chars[i] = (char) getVarint();
                }
                return new String(chars);
            }
            case 5:
                return new String(getBytes(), StandardCharsets.ISO_8859_1);
            default:
                throw new IllegalArgumentException("Invalid string encoding");
        }
    }

    private byte[] getBytes() {
        byte[] bytes = new byte[getVarint()];
        buf.get(bytes);
        return bytes;
    }

    private void seek(long pos) throws IOException {
        filePosition = pos;
        ch.position(pos);
        buf.rewind().flip();
    }

    private boolean ensureBytes(int needed) throws IOException {
        if (buf.remaining() >= needed) {
            return true;
        }

        filePosition += buf.position();

        if (buf.capacity() < needed) {
            ByteBuffer newBuf = ByteBuffer.allocateDirect(needed);
            newBuf.put(buf);
            buf = newBuf;
        } else {
            buf.compact();
        }

        while (ch.read(buf) > 0 && buf.position() < needed) {
            // keep reading
        }
        buf.flip();
        return buf.limit() > 0;
    }
}
