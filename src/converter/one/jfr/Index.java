/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr;

/**
 * Fast and compact Object->int map.
 */
public class Index<T> {
    protected static final int INITIAL_CAPACITY = 16 * 1024;

    private Object[] keys;
    private int[] values;

    public int size;

    public Index() {
        this(INITIAL_CAPACITY);
    }

    public Index(int initialCapacity) {
        this.keys = new Object[initialCapacity];
        this.values = new int[initialCapacity];
    }

    public int index(T key) {
        if (key == null) {
            throw new NullPointerException("Null key is not allowed");
        }

        int mask = keys.length - 1;
        int i = hashCode(key) & mask;
        while (true) {
            Object currentKey = keys[i];
            if (currentKey == null) {
                break;
            }
            if (currentKey.equals(key)) {
                return values[i];
            }
            i = (i + 1) & mask;
        }
        insertAt(i, key);
        return size;
    }

    protected void insertAt(int pos, T key) {
        keys[pos] = key;
        values[pos] = ++size;

        if (size * 2 > keys.length) {
            resize(keys.length * 2);
        }
    }

    @SuppressWarnings("unchecked")
    public void orderedKeys(T[] out) {
        for (int i = 0; i < keys.length; i++) {
            Object key = keys[i];
            if (key != null) {
                out[values[i] - 1] = (T) key;
            }
        }
    }

    public int size() {
        return size;
    }

    @SuppressWarnings("unchecked")
    protected void resize(int newCapacity) {
        Object[] newKeys = new Object[newCapacity];
        int[] newValues = new int[newCapacity];
        int mask = newKeys.length - 1;

        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != null) {
                for (int j = hashCode((T) keys[i]) & mask; ; j = (j + 1) & mask) {
                    if (newKeys[j] == null) {
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

    protected int hashCode(T key) {
        return key.hashCode() * 0x5bd1e995;
    }

}
