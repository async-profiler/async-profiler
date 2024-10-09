/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;

public class LzNodeTree {

    private static final int INITIAL_CAPACITY = 2 * 1024 * 1024;

    // hash(methodId << 32 | parentNodeId) -> methodId << 32 | parentNodeId
    private long[] keys;    // reused by SynonymTable
    // hash(methodId << 32 | parentNodeId) -> childNodeId
    private int[] values;  // can be reused after buildLz78TreeAndPrepareData

    // (nodeId - 1) -> methodId << 32 | parentNodeId
    private long[] outputData;              // can be reused after writeTree:130!
    // nodeId -> childrenCount
    private int[] childrenCount;    // reused by SynonymTable
    // nodeId -> parentNodeId << 32 | lengthToRoot
    private long[] lengthToRoot;

    private int storageSize = 0;
    private int nodesCount = 1;

    public LzNodeTree() {
        keys = new long[INITIAL_CAPACITY];
        values = new int[INITIAL_CAPACITY];

        outputData = new long[INITIAL_CAPACITY / 2];
        childrenCount = new int[INITIAL_CAPACITY / 2];
        lengthToRoot = new long[INITIAL_CAPACITY / 2];
    }

    public int appendChild(int parentNode, int methodId) {
        long method = (long) methodId << 32;
        long key = method | parentNode;

        int mask = keys.length - 1;
        int i = hashCode(key) & mask;
        while (true) {
            long k = keys[i];
            if (k == 0) {
                break;
            }
            if (k == key) {
                return values[i];
            }
            i = (i + 1) & mask;
        }

        if (nodesCount >= outputData.length) {
            outputData = Arrays.copyOf(outputData, nodesCount + nodesCount / 2);
            childrenCount = Arrays.copyOf(childrenCount, nodesCount + nodesCount / 2);
            lengthToRoot = Arrays.copyOf(lengthToRoot, nodesCount + nodesCount / 2);
        }

        lengthToRoot[nodesCount] = ((int) lengthToRoot[parentNode] + 1) | ((long) parentNode << 32);
        outputData[nodesCount - 1] = key;
        keys[i] = key;
        values[i] = nodesCount;

        if (nodesCount * 2 > keys.length) {
            resize(keys.length * 2);
        }
        nodesCount++;

        childrenCount[parentNode]--; // negotiation for better sort

        return 0;
    }

    public long[] treeData() {
        return outputData;
    }

    public int treeDataSize() {
        return nodesCount - 1;
    }

    public int extractParentId(long treeElement) {
        return (int) treeElement;
    }

    public int extractMethodId(long treeElement) {
        return (int) (treeElement >>> 32);
    }

    public void markNodeAsLastlyUsed(int nodeId) {
        long ltr = lengthToRoot[nodeId];
        int parent = (int) (ltr >>> 32);
        if (parent >= 0) {
            lengthToRoot[nodeId] = ltr | 0x8000000000000000L;
            do {
                ltr = lengthToRoot[parent];
                lengthToRoot[parent] = ltr | 0xC000000000000000L;
                parent = (int) (ltr >>> 32);
            } while (parent > 0);
        }
    }

    // destroys values
    public void compactTree(int[] remapAsWell, int fromIndex, int toIndex) {
        int[] mappings = values;
        mappings[0] = 0;
        int nodes = 1;
        int storageSize = 0;
        for (int oldNodeID = 1; oldNodeID < nodesCount; oldNodeID++) {
            long ltr = lengthToRoot[oldNodeID];
            if (ltr >= 0) {
                // unused
                continue;
            }
            if ((ltr & 0x4000000000000000L) == 0) {
                storageSize += (int) ltr;
            }
            mappings[oldNodeID] = nodes;
            childrenCount[nodes] = childrenCount[oldNodeID];
            long out = outputData[oldNodeID - 1];
            long outMethod = 0xFFFFFFFF00000000L & out;
            int oldParent = (int) out;
            outputData[nodes - 1] = outMethod | mappings[oldParent];
            nodes++;
        }
        for (int i = fromIndex; i < toIndex; i++) {
            remapAsWell[i] = mappings[remapAsWell[i]];
        }
        this.storageSize = storageSize;
        this.nodesCount = nodes;
    }

    // destroys keys and childrenCount arrays
    public SynonymTable extractSynonymTable() {
        return new SynonymTable(keys, childrenCount, nodesCount);
    }

    public int storageSize() {
        return storageSize;
    }

    public int nodesCount() {
        return nodesCount;
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
}
