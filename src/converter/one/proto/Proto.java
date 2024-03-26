/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.proto;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/**
 * Simplified implementation of Protobuf writer, capable of encoding
 * varints, doubles, ASCII strings and embedded messages
 */
public class Proto {
    private byte[] buf;
    private int pos;

    public Proto(int capacity) {
        this.buf = new byte[capacity];
    }

    public byte[] buffer() {
        return buf;
    }

    public int size() {
        return pos;
    }

    public void reset() {
        pos = 0;
    }

    public Proto field(int index, int n) {
        tag(index, 0);
        writeInt(n);
        return this;
    }

    public Proto field(int index, long n) {
        tag(index, 0);
        writeLong(n);
        return this;
    }

    public Proto field(int index, double d) {
        tag(index, 1);
        writeDouble(d);
        return this;
    }

    public Proto field(int index, String s) {
        tag(index, 2);
        byte[] bytes = s.getBytes(StandardCharsets.UTF_8);
        writeBytes(bytes, 0, bytes.length);
        return this;
    }

    public Proto field(int index, byte[] bytes) {
        tag(index, 2);
        writeBytes(bytes, 0, bytes.length);
        return this;
    }

    public Proto field(int index, Proto proto) {
        tag(index, 2);
        writeBytes(proto.buf, 0, proto.pos);
        return this;
    }

    public int startField(int index) {
        tag(index, 2);
        ensureCapacity(3);
        return pos += 3;
    }

    public void commitField(int mark) {
        int length = pos - mark;
        if (length >= 1 << (7 * 3)) {
            throw new IllegalArgumentException("Field too large");
        }

        buf[mark - 3] = (byte) (0x80 | (length & 0x7f));
        buf[mark - 2] = (byte) (0x80 | ((length >>> 7) & 0x7f));
        buf[mark - 1] = (byte) (length >>> 14);
    }

    public void writeInt(int n) {
        int length = n == 0 ? 1 : (38 - Integer.numberOfLeadingZeros(n)) / 7;
        ensureCapacity(length);

        while ((n >>> 7) != 0) {
            buf[pos++] = (byte) (0x80 | (n & 0x7f));
            n >>>= 7;
        }
        buf[pos++] = (byte) n;
    }

    public void writeLong(long n) {
        int length = n == 0 ? 1 : (70 - Long.numberOfLeadingZeros(n)) / 7;
        ensureCapacity(length);

        while ((n >>> 7) != 0) {
            buf[pos++] = (byte) (0x80 | (n & 0x7f));
            n >>>= 7;
        }
        buf[pos++] = (byte) n;
    }

    public void writeDouble(double d) {
        ensureCapacity(8);
        long n = Double.doubleToRawLongBits(d);
        buf[pos] = (byte) n;
        buf[pos + 1] = (byte) (n >>> 8);
        buf[pos + 2] = (byte) (n >>> 16);
        buf[pos + 3] = (byte) (n >>> 24);
        buf[pos + 4] = (byte) (n >>> 32);
        buf[pos + 5] = (byte) (n >>> 40);
        buf[pos + 6] = (byte) (n >>> 48);
        buf[pos + 7] = (byte) (n >>> 56);
        pos += 8;
    }

    public void writeBytes(byte[] bytes, int offset, int length) {
        writeInt(length);
        ensureCapacity(length);
        System.arraycopy(bytes, offset, buf, pos, length);
        pos += length;
    }

    private void tag(int index, int type) {
        ensureCapacity(1);
        buf[pos++] = (byte) (index << 3 | type);
    }

    private void ensureCapacity(int length) {
        if (pos + length > buf.length) {
            int newLength = buf.length * 2;
            buf = Arrays.copyOf(buf, newLength < 0 ? 0x7ffffff0 : Math.max(newLength, pos + length));
        }
    }
}
