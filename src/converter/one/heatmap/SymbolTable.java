package one.heatmap;

import java.util.Arrays;

public class SymbolTable {

    private static final int INITIAL_CAPACITY = 16 * 1024;
    private static final long MARK_POST_TRANSFORM = 1L << 32;
    private static final byte[] UNKNOWN = "<Unknown>".getBytes();

    private static final byte[] BYTE = "byte".getBytes();
    private static final byte[] CHAR = "char".getBytes();
    private static final byte[] SHORT = "short".getBytes();
    private static final byte[] INT = "int".getBytes();
    private static final byte[] LONG = "long".getBytes();
    private static final byte[] BOOLEAN = "boolean".getBytes();
    private static final byte[] FLOAT = "float".getBytes();
    private static final byte[] DOUBLE = "double".getBytes();

    private byte[][] keys;
    // highest 31 bits for index, lowest 32 bits for hash, post_transform mark between them
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

    public int indexWithPostTransform(byte[] key) {
        return index(key, MARK_POST_TRANSFORM);
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
            if (hash == hashCode && (currentMeta & MARK_POST_TRANSFORM) == markPostTransform) {
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

    public byte[][] orderedKeys() {
        byte[][] out = new byte[size][];

        for (int i = 0; i < meta.length; i++) {
            long currentMeta = meta[i];
            if (currentMeta == 0) {
                continue;
            }

            int index = (int) (currentMeta >>> 33);

            if ((currentMeta & MARK_POST_TRANSFORM) == MARK_POST_TRANSFORM) {
                out[index - 1] = convertClassName(keys[i]);
            } else {
                out[index - 1] = keys[i];
            }
        }

        return out;
    }


    private byte[] convertClassName(byte[] className) {
        if (className.length == 0) {
            return className;
        }
        int arrayDepth = 0;
        while (className[arrayDepth] == '[') {
            arrayDepth++;
        }

        if (arrayDepth == 0) {
            return replaceSlashes(className);
        }

        switch (className[arrayDepth]) {
            case 'B':
                return addParentheses(BYTE, arrayDepth);
            case 'C':
                return addParentheses(CHAR, arrayDepth);
            case 'S':
                return addParentheses(SHORT, arrayDepth);
            case 'I':
                return addParentheses(INT, arrayDepth);
            case 'J':
                return addParentheses(LONG, arrayDepth);
            case 'Z':
                return addParentheses(BOOLEAN, arrayDepth);
            case 'F':
                return addParentheses(FLOAT, arrayDepth);
            case 'D':
                return addParentheses(DOUBLE, arrayDepth);
            case 'L':
                return addParentheses(className, arrayDepth + 1, className.length - arrayDepth - 2, arrayDepth);
            default:
                return addParentheses(className, arrayDepth, className.length - arrayDepth, arrayDepth);
        }
    }

    private byte[] addParentheses(byte[] name, int arrayDepth) {
        return addParentheses(name, 0, name.length, arrayDepth);
    }

    private byte[] addParentheses(byte[] name, int start, int length, int arrayDepth) {
        byte[] result = new byte[length + arrayDepth * 2];
        for (int i = 0; i < length; i++) {
            result[i] = replace(name[i + start]);
        }
        while (length < result.length) {
            result[length++] = '[';
            result[length++] = ']';
        }
        return result;
    }

    private byte replace(byte ch) {
        return ch == (byte)'/' ? (byte) '.' : ch;
    }

    private byte[] replaceSlashes(byte[] className) {
        for (int i = 0; i < className.length; i++) {
            if (className[i] == (byte)'/') {
                className[i] = '.';
            }
        }
        return className;
    }

}
