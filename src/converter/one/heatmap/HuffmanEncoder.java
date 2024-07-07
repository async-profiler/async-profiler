/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;
import java.util.PriorityQueue;

public class HuffmanEncoder {

    private final long[] decodeTable;   // 8 bit for bits count, 56 value
    private final long[] encodeTable;   // 8 bit for bits count, 56 bits

    private int data;
    private int bits;

    // log2(123^9) = 62.4826305481 > 62 bits, 0.7% space lost, but it is expensive to decode (no support for int64 in js)
    // log2(123^4) = 27.7700580214 > 27 bits, 2.8% space lost, but it is cheap to decode (using one int32)
    private static final int MAX_BITS = 27;
    public final int[] values = new int[4]; // 0..122

    public HuffmanEncoder(int[] frequencies, int maxFrequencyIndex) {
        PriorityQueue<Node> minHeap = new PriorityQueue<>(maxFrequencyIndex + 1);
        for (int i = 0; i <= maxFrequencyIndex; i++) {
            int frequency = frequencies[i];
            if (frequency == 0) {
                continue;
            }
            minHeap.add(new Node(frequency, i));
        }

        while (minHeap.size() > 1) {
            Node left = minHeap.remove();
            Node right = minHeap.remove();

            minHeap.add(new Node(left, right));
        }

        long[] decodeTable = new long[maxFrequencyIndex + 1];
        minHeap.remove().fillTable(decodeTable, 0);
        Arrays.sort(decodeTable);
        for (int i = 0; i < decodeTable.length; i++) {
            if (decodeTable[i] != 0) {
                if (i != 0) {
                    long[] nextDecodeTable = new long[decodeTable.length - i];
                    System.arraycopy(decodeTable, i, nextDecodeTable, 0, nextDecodeTable.length);
                    decodeTable = nextDecodeTable;
                }
                break;
            }
        }
        this.decodeTable = decodeTable;

        encodeTable = new long[maxFrequencyIndex + 1];
        encodeTable[(int) decodeTable[0]] = decodeTable[0] & 0xFF00_0000_0000_0000L;
        long code = 0;

        for (int i = 1; i < decodeTable.length; i++) {
            long decodePrev = decodeTable[i - 1];
            long decodeNow = decodeTable[i];

            long prevCount = decodePrev >>> 56;
            long nowCount = decodeNow >>> 56;

            code = (code + 1) << (nowCount - prevCount);

            int value = (int) decodeNow;
            encodeTable[value] = nowCount << 56 | code;
        }
    }

    public boolean append(int value) {
        boolean hasOverflow = false;

        long v = encodeTable[value];
        int bits = (int) (v >>> 56);
        for (long i = 1L << (bits - 1); i > 0; i >>>= 1) {
            this.data = this.data << 1 | ((v & i) == 0 ? 0 : 1);
            if (++this.bits == MAX_BITS) {
                hasOverflow = true;
                flush();
            }
        }

        return hasOverflow;
    }

    public boolean flushIfNeed() {
        if (bits == 0) {
            return false;
        }
        this.data = this.data << (MAX_BITS - bits);
        flush();
        return true;
    }

    public void flush() {
        data = Integer.reverse(data) >>> 5;

        values[3] = data % 123;
        data /= 123;
        values[2] = data % 123;
        data /= 123;
        values[1] = data % 123;
        data /= 123;
        values[0] = data;
        data = 0;

        bits = 0;
    }

    public long[] calculateOutputTable() {
        return decodeTable;
    }

    private static class Node implements Comparable<Node> {
        final int frequency;
        final int value;

        Node left, right;

        Node(int frequency, int value) {
            this.frequency = frequency;
            this.value = value;
        }

        public Node(Node left, Node right) {
            this.left = left;
            this.right = right;
            this.frequency = left.frequency + right.frequency;
            this.value = -1;
        }

        public void fillTable(long[] table, long bitsCount) {
            if (value >= 0) {
                table[value] = bitsCount | value;
                return;
            }
            left.fillTable(table, bitsCount + 0x0100_0000_0000_0000L);
            right.fillTable(table, bitsCount + 0x0100_0000_0000_0000L);
        }

        @Override
        public int compareTo(Node o) {
            // frequencies are strictly positive
            return frequency - o.frequency;
        }
    }

}
