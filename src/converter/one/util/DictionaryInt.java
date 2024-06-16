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
 * Fast and compact long->long map.
 */
public class DictionaryInt {
    private static final int INITIAL_CAPACITY = 16;

    private long[] keys;
    private int[] values;
    private int size;

    public DictionaryInt() {
        this.keys = new long[INITIAL_CAPACITY];
        this.values = new int[INITIAL_CAPACITY];
    }

    public void clear() {
        keys = new long[INITIAL_CAPACITY];
        values = new int[INITIAL_CAPACITY];
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

    public int putIfAbsent(long key, int value) {
        if (key == 0) {
            throw new IllegalArgumentException("Zero key not allowed");
        }

        int mask = keys.length - 1;
        int i = hashCode(key) & mask;
        while (keys[i] != 0) {
            if (keys[i] == key) {
                return values[i];
            }
            i = (i + 1) & mask;
        }
        keys[i] = key;
        values[i] = value;

        if (++size * 2 > keys.length) {
            resize(keys.length * 2);
        }
        return value;
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

    public int size() {
        return size;
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
