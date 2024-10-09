/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import one.convert.Arguments;
import one.convert.JfrConverter;

public class SymbolTable {

    private static final int INITIAL_CAPACITY = 16 * 1024;
    private static final long MARK_JAVA_CLASS = 1L << 32;
    private static final byte[] UNKNOWN = "<Unknown>".getBytes();

    private byte[][] keys;
    // highest 31 bits for index, lowest 32 bits for hash, mark_java_class mark between them
    private long[] meta;

    private int size;

    public SymbolTable() {
        this(INITIAL_CAPACITY);
    }

    public SymbolTable(int initialCapacity) {
        this.keys = new byte[initialCapacity][];
        this.meta = new long[initialCapacity];
    }

    public int index(byte[] key) {
        return index(key, 0);
    }

    public int indexForJavaClass(byte[] key) {
        return index(key, MARK_JAVA_CLASS);
    }

    private int index(byte[] key, long markPostTransform) {
        if (key == null) {
            key = UNKNOWN;
        }

        int mask = keys.length - 1;
        int hashCode = murmur(key);
        int i = hashCode & mask;
        while (true) {
            long currentMeta = meta[i];
            if (currentMeta == 0) {
                break;
            }

            int hash = (int) currentMeta;
            if (hash == hashCode && (currentMeta & MARK_JAVA_CLASS) == markPostTransform) {
                if (Arrays.equals(keys[i], key)) {
                    return (int) (currentMeta >>> 33);
                }
            }

            i = (i + 1) & mask;
        }

        size++;
        keys[i] = key;
        meta[i] = (long) size << 33 | (hashCode & 0xFFFFFFFFL) | markPostTransform;

        if (size * 2 > keys.length) {
            resize(keys.length * 2);
        }

        return size;
    }

    private void resize(int newCapacity) {
        byte[][] newKeys = new byte[newCapacity][];
        long[] newMeta = new long[newCapacity];
        int mask = newMeta.length - 1;

        for (int i = 0; i < meta.length; i++) {
            long currentMeta = meta[i];
            if (currentMeta != 0) {
                int hashCode = (int) currentMeta;
                for (int j = hashCode & mask; ; j = (j + 1) & mask) {
                    if (newMeta[j] == 0) {
                        newMeta[j] = meta[i];
                        newKeys[j] = keys[i];
                        break;
                    }
                }
            }
        }

        keys = newKeys;
        meta = newMeta;
    }

    private static int murmur(byte[] data) {
        int m = 0x5bd1e995;
        int h = 0x9747b28c ^ data.length;

        int limit = data.length & ~3;

        for (int i = 0; i < limit; i += 4) {
            int k = (data[i] & 0xff) |
                (data[i + 1] & 0xff) << 8 |
                (data[i + 2] & 0xff) << 16 |
                data[i + 3] << 24;
            k *= m;
            k ^= k >>> 24;
            k *= m;
            h *= m;
            h ^= k;
        }

        switch (data.length % 4) {
            case 3:
                h ^= (data[data.length - 3] & 0xff) << 16;
            case 2:
                h ^= (data[data.length - 2] & 0xff) << 8;
            case 1:
                h ^= (data[data.length - 1] & 0xff);
                h *= m;
        }

        h ^= h >>> 13;
        h *= m;
        h ^= h >>> 15;

        return h;
    }

    public String[] orderedKeys(Arguments args) {
        String[] out = new String[size];

        for (int i = 0; i < meta.length; i++) {
            long currentMeta = meta[i];
            if (currentMeta == 0) {
                continue;
            }

            int index = (int) (currentMeta >>> 33);

            if ((currentMeta & MARK_JAVA_CLASS) == MARK_JAVA_CLASS) {
                out[index - 1] = convertClassName(keys[i], args);
            } else {
                out[index - 1] = new String(keys[i], StandardCharsets.UTF_8);
            }
        }

        return out;
    }

    private String convertClassName(byte[] className, Arguments args) {
        return JfrConverter.convertJavaClassName(className, args);
    }

}
