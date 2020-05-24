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
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Parses JFR output produced by async-profiler.
 * Note: this class is not supposed to read JFR files produced by other tools.
 */
public class JfrReader implements Closeable {

    private static final int
            CONTENT_THREAD = 7,
            CONTENT_STACKTRACE = 9,
            CONTENT_CLASS = 10,
            CONTENT_METHOD = 32,
            CONTENT_SYMBOL = 33,
            CONTENT_STATE = 34,
            CONTENT_FRAME_TYPE = 47;

    private static final int
            EVENT_EXECUTION_SAMPLE = 20;

    private final FileChannel ch;
    private final ByteBuffer buf;

    public final long startNanos;
    public final long stopNanos;
    public final Map<Integer, Frame[]> stackTraces = new HashMap<>();
    public final Map<Long, MethodRef> methods = new HashMap<>();
    public final Map<Long, ClassRef> classes = new HashMap<>();
    public final Map<Long, byte[]> symbols = new HashMap<>();
    public final Map<Integer, byte[]> threads = new HashMap<>();
    public final List<Sample> samples = new ArrayList<>();

    public JfrReader(String fileName) throws IOException {
        this.ch = FileChannel.open(Paths.get(fileName), StandardOpenOption.READ);
        this.buf = ch.map(FileChannel.MapMode.READ_ONLY, 0, ch.size());

        int checkpointOffset = buf.getInt(buf.capacity() - 4);
        this.startNanos = buf.getLong(buf.capacity() - 24);
        this.stopNanos = buf.getLong(checkpointOffset + 8);

        readCheckpoint(checkpointOffset);
        readEvents(checkpointOffset);
    }

    @Override
    public void close() throws IOException {
        ch.close();
    }

    private void readEvents(int checkpointOffset) {
        buf.position(16);

        while (buf.position() < checkpointOffset) {
            int size = buf.getInt();
            int type = buf.getInt();
            if (type == EVENT_EXECUTION_SAMPLE) {
                long time = buf.getLong();
                int tid = buf.getInt();
                int stackTraceId = (int) buf.getLong();
                short threadState = buf.getShort();
                samples.add(new Sample(time, tid, stackTraceId, threadState));
            } else {
                buf.position(buf.position() + size - 8);
            }
        }

        Collections.sort(samples);
    }

    private void readCheckpoint(int checkpointOffset) {
        buf.position(checkpointOffset + 24);

        readFrameTypes();
        readThreadStates();
        readStackTraces();
        readMethods();
        readClasses();
        readSymbols();
        readThreads();
    }

    private void readFrameTypes() {
        int count = getTableSize(CONTENT_FRAME_TYPE);
        for (int i = 0; i < count; i++) {
            buf.get();
            getSymbol();
        }
    }

    private void readThreadStates() {
        int count = getTableSize(CONTENT_STATE);
        for (int i = 0; i < count; i++) {
            buf.getShort();
            getSymbol();
        }
    }

    private void readStackTraces() {
        int count = getTableSize(CONTENT_STACKTRACE);
        for (int i = 0; i < count; i++) {
            int id = (int) buf.getLong();
            byte truncated = buf.get();
            Frame[] frames = new Frame[buf.getInt()];
            for (int j = 0; j < frames.length; j++) {
                long method = buf.getLong();
                int bci = buf.getInt();
                byte type = buf.get();
                frames[j] = new Frame(method, type);
            }
            stackTraces.put(id, frames);
        }
    }

    private void readMethods() {
        int count = getTableSize(CONTENT_METHOD);
        for (int i = 0; i < count; i++) {
            long id = buf.getLong();
            long cls = buf.getLong();
            long name = buf.getLong();
            long sig = buf.getLong();
            short modifiers = buf.getShort();
            byte hidden = buf.get();
            methods.put(id, new MethodRef(cls, name, sig));
        }
    }

    private void readClasses() {
        int count = getTableSize(CONTENT_CLASS);
        for (int i = 0; i < count; i++) {
            long id = buf.getLong();
            long loader = buf.getLong();
            long name = buf.getLong();
            short modifiers = buf.getShort();
            classes.put(id, new ClassRef(name));
        }
    }

    private void readSymbols() {
        int count = getTableSize(CONTENT_SYMBOL);
        for (int i = 0; i < count; i++) {
            long id = buf.getLong();
            byte[] symbol = getSymbol();
            symbols.put(id, symbol);
        }
    }

    private void readThreads() {
        int count = getTableSize(CONTENT_THREAD);
        for (int i = 0; i < count; i++) {
            int id = buf.getInt();
            byte[] name = getSymbol();
            threads.put(id, name);
        }
    }

    private int getTableSize(int contentType) {
        if (buf.getInt() != contentType) {
            throw new IllegalArgumentException("Expected content type " + contentType);
        }
        return buf.getInt();
    }

    private byte[] getSymbol() {
        byte[] symbol = new byte[buf.getShort() & 0xffff];
        buf.get(symbol);
        return symbol;
    }
}
