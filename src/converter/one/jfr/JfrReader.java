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
    private static final int CHUNK_HEADER_SIZE = 68;
    private static final int CPOOL_OFFSET = 16;
    private static final int META_OFFSET = 24;

    private final FileChannel ch;
    private final ByteBuffer buf;

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
    public final List<Sample> samples = new ArrayList<>();

    public JfrReader(String fileName) throws IOException {
        this.ch = FileChannel.open(Paths.get(fileName), StandardOpenOption.READ);
        this.buf = ch.map(FileChannel.MapMode.READ_ONLY, 0, ch.size());

        if (buf.getInt(0) != 0x464c5200) {
            throw new IOException("Not a valid JFR file");
        }

        int version = buf.getInt(4);
        if (version < 0x20000 || version > 0x2ffff) {
            throw new IOException("Unsupported JFR version: " + (version >>> 16) + "." + (version & 0xffff));
        }

        this.startNanos = buf.getLong(32);
        this.durationNanos = buf.getLong(40);
        this.startTicks = buf.getLong(48);
        this.ticksPerSec = buf.getLong(56);

        readMeta();
        readConstantPool();
        readEvents();
    }

    @Override
    public void close() throws IOException {
        ch.close();
    }

    private void readMeta() {
        buf.position(buf.getInt(META_OFFSET + 4));
        getVarint();
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

    private void readConstantPool() {
        int offset = buf.getInt(CPOOL_OFFSET + 4);
        while (true) {
            buf.position(offset);
            getVarint();
            getVarint();
            getVarlong();
            getVarlong();
            long delta = getVarlong();
            getVarint();

            int poolCount = getVarint();
            for (int i = 0; i < poolCount; i++) {
                int type = getVarint();
                readConstants(types.get(type));
            }

            if (delta == 0) {
                break;
            }
            offset += delta;
        }
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

    private void readEvents() {
        int executionSample = getTypeId("jdk.ExecutionSample");
        int nativeMethodSample = getTypeId("jdk.NativeMethodSample");

        buf.position(CHUNK_HEADER_SIZE);
        while (buf.hasRemaining()) {
            int position = buf.position();
            int size = getVarint();
            int type = getVarint();
            if (type == executionSample || type == nativeMethodSample) {
                readExecutionSample();
            } else {
                buf.position(position + size);
            }
        }

        Collections.sort(samples);
    }

    private void readExecutionSample() {
        long time = getVarlong();
        int tid = getVarint();
        int stackTraceId = getVarint();
        int threadState = getVarint();
        samples.add(new Sample(time, tid, stackTraceId, threadState));

        StackTrace stackTrace = stackTraces.get(stackTraceId);
        if (stackTrace != null) {
            stackTrace.samples++;
        }
    }

    private int getTypeId(String typeName) {
        JfrClass type = typesByName.get(typeName);
        return type != null ? type.id : -1;
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
}
