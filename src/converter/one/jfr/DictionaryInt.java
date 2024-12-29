/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr;

import java.util.Arrays;

/**
 * Fast and compact long->int map.
 */
public class DictionaryInt {
    private static final int INITIAL_CAPACITY = 16;

    private long[] keys;
    private int[] values;
    private int size;

    public DictionaryInt() {
        this(INITIAL_CAPACITY);
    }

    public DictionaryInt(int initialCapacity) {
        this.keys = new long[initialCapacity];
        this.values = new int[initialCapacity];
    }

    public void clear() {
        Arrays.fill(keys, 0);
        Arrays.fill(values, 0);
        size = 0;
    }

    public void put(long key, int value) {
        if (key == 0) {
            throw new IllegalArgumentException("Zero key not allowed");
        }

        int mask = keys.length - 1;
        int i = hashCode(key) & mask;
        while (keys[i] != 0) {
            if (keys[i] == key) {
                values[i] = value;
                return;
            }
            i = (i + 1) & mask;
        }
        keys[i] = key;
        values[i] = value;

        if (++size * 2 > keys.length) {
            resize(keys.length * 2);
        }
    }

    public int get(long key) {
        int mask = keys.length - 1;
        int i = hashCode(key) & mask;
        while (keys[i] != key) {
            if (keys[i] == 0) {
                throw new IllegalArgumentException("No such key: " + key);
            }
            i = (i + 1) & mask;
        }
        return values[i];
    }

    public int get(long key, int notFound) {
        int mask = keys.length - 1;
        int i = hashCode(key) & mask;
        while (keys[i] != key) {
            if (keys[i] == 0) {
                return notFound;
            }
            i = (i + 1) & mask;
        }
        return values[i];
    }

    public void forEach(Visitor visitor) {
        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != 0) {
                visitor.visit(keys[i], values[i]);
            }
        }
    }

    public int preallocate(int count) {
        if (count * 2 > keys.length) {
            resize(Integer.highestOneBit(count * 4 - 1));
        }
        return count;
    }

    private void resize(int newCapacity) {
        long[] newKeys = new long[newCapacity];
        int[] newValues = new int[newCapacity];
        int mask = newKeys.length - 1;

        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != 0) {
                for (int j = hashCode(keys[i]) & mask; ; j = (j + 1) & mask) {
                    if (newKeys[j] == 0) {
                        newKeys[j] = keys[i];
                        newValues[j] = values[i];
                        break;
                    }
                }
            }
        }

        keys = newKeys;
        values = newValues;
    }

    private static int hashCode(long key) {
        key *= 0xc6a4a7935bd1e995L;
        return (int) (key ^ (key >>> 32));
    }

    public interface Visitor {
        void visit(long key, int value);
    }
}
