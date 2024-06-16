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

package one.util;

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
            //noinspection unchecked
            if (equals((T) currentKey, key)) {
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

    public int preallocate(int count) {
        if (count * 2 > keys.length) {
            resize(Integer.highestOneBit(count * 4 - 1));
        }
        return count;
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

    protected boolean equals(T k1, T k2) {
        return k1.equals(k2);
    }

    protected int hashCode(T key) {
        return key.hashCode() * 0x5bd1e995;
    }

}
