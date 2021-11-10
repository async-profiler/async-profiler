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

package one.proto;

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

    public Proto field(int index, double d) {
        tag(index, 1);
        writeDouble(d);
        return this;
    }

    public Proto field(int index, String s) {
        tag(index, 2);
        writeString(s);
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

    public void writeInt(int n) {
        int length = n == 0 ? 1 : (38 - Integer.numberOfLeadingZeros(n)) / 7;
        ensureCapacity(length);

        while (n > 0x7f) {
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

    public void writeString(String s) {
        int length = s.length();
        writeInt(length);
        ensureCapacity(length);

        for (int i = 0; i < length; i++) {
            buf[pos++] = (byte) s.charAt(i);
        }
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
            buf = Arrays.copyOf(buf, Math.max(pos + length, buf.length * 2));
        }
    }
}
