/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;

public class StackStorage {

    protected static final int INITIAL_CAPACITY = 16 * 1024;

    private int size;

    // highest 32 bits for index, lowest 32 bits for hash
    private long[] meta;

    // ordered incrementally
    private int[][] values;

    public StackStorage() {
        this(INITIAL_CAPACITY);
    }

    public StackStorage(int initialCapacity) {
        meta = new long[initialCapacity * 2];
        values = new int[initialCapacity][];
    }

    public int[] get(int id) {
        return values[id - 1];
    }

    public int index(int[] stack, int stackSize) {
        int mask = meta.length - 1;
        int hashCode = murmur(stack, stackSize);
        int i = hashCode & mask;
        while (true) {
            long currentMeta = meta[i];
            if (currentMeta == 0) {
                break;
            }

            int hash = (int) currentMeta;
            if (hash == hashCode) {
                int index = (int) (currentMeta >>> 32);
                int[] value = values[index];
                if (equals(value, stack, stackSize)) {
                    return index + 1;
                }
            }

            i = (i + 1) & mask;
        }

        values[size] = Arrays.copyOf(stack, stackSize);
        meta[i] = (long) size << 32 | (hashCode & 0xFFFFFFFFL);
        size++;

        if (size * 2 > values.length) {
            resize(values.length * 2);
        }

        return size;
    }

    public int indexWithPrototype(int[] prototype, int append) {
        int mask = meta.length - 1;
        int hashCode = murmurWithExtra(prototype, append);
        int i = hashCode & mask;
        while (true) {
            long currentMeta = meta[i];
            if (currentMeta == 0) {
                break;
            }

            int hash = (int) currentMeta;
            if (hash == hashCode) {
                int index = (int) (currentMeta >>> 32);
                int[] value = values[index - 1];
                if (equalsWithExtra(value, prototype, append)) {
                    return index;
                }
            }

            i = (i + 1) & mask;
        }

        int[] stack = Arrays.copyOf(prototype, prototype.length + 1);
        stack[prototype.length] = append;
        values[size] = stack;
        meta[i] = (long) size << 32 | (hashCode & 0xFFFFFFFFL);
        size++;

        if (size * 2 > values.length) {
            resize(values.length * 2);
        }

        return size;
    }

    public int[][] orderedTraces() {
        return Arrays.copyOf(values, size);
    }

    protected void resize(int newCapacity) {
        long[] newMeta = new long[newCapacity * 2];
        int mask = newMeta.length - 1;

        for (long m : meta) {
            if (m != 0) {
                int hashCode = (int) m;
                for (int j = hashCode & mask; ; j = (j + 1) & mask) {
                    if (newMeta[j] == 0) {
                        newMeta[j] = m;
                        break;
                    }
                }
            }
        }

        meta = newMeta;
        values = Arrays.copyOf(values, newCapacity);
    }

    private boolean equals(int[] a, int[] b, int bSize) {
        if (a.length != bSize) {
            return false;
        }
        for (int i = 0; i < bSize; i++) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return true;
    }

    private boolean equalsWithExtra(int[] a, int[] b, int extra) {
        if (a.length != b.length + 1) {
            return false;
        }
        for (int i = 0; i < b.length; i++) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return a[b.length] == extra;
    }

    private static int murmur(int[] data, int size) {
        int m = 0x5bd1e995;
        int h = 0x9747b28c ^ data.length;

        for (int i = 0; i < size; i++) {
            int k = data[i];
            k *= m;
            k ^= k >>> 24;
            k *= m;
            h *= m;
            h ^= k;
        }

        h ^= h >>> 13;
        h *= m;
        h ^= h >>> 15;

        return h;
    }

    private static int murmurWithExtra(int[] data, int extra) {
        int m = 0x5bd1e995;
        int h = 0x9747b28c ^ (data.length + 1);

        for (int k : data) {
            k *= m;
            k ^= k >>> 24;
            k *= m;
            h *= m;
            h ^= k;
        }

        int k = extra * m;
        k ^= k >>> 24;
        k *= m;
        h *= m;
        h ^= k;

        h ^= h >>> 13;
        h *= m;
        h ^= h >>> 15;

        return h;
    }
}
