/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;

public class SynonymTable {

    private final long[] synonyms;
    private final int[] childrenCountOrNodeSynonym;
    private final int nodesCount;

    private int synonymsCount;

    public SynonymTable(long[] synonyms, int[] childrenCount, int nodesCount) {
        this.synonyms = synonyms;
        this.childrenCountOrNodeSynonym = childrenCount;
        this.nodesCount = nodesCount;
    }

    public void calculateSynonyms() {
        int[] childrenCount = childrenCountOrNodeSynonym;
        for (int i = 0; i < nodesCount; i++) {
            synonyms[i] = (long) childrenCount[i] << 32 | i;
        }

        Arrays.sort(synonyms, 0, nodesCount);

        synonymsCount = Math.min(61 * 61, nodesCount);

        int[] nodeSynonyms = childrenCountOrNodeSynonym;
        for (int i = 0; i < nodesCount; i++) {
            nodeSynonyms[i] = synonymsCount + i;
        }
        for (int i = 0; i < synonymsCount; i++) {
            nodeSynonyms[(int) (synonyms[i] & 0xFFFFFFFFL)] = i;
        }
    }

    public int synonymsCount() {
        return synonymsCount;
    }

    public int synonymAt(int synonymIndex) {
        return (int) (synonyms[synonymIndex] & 0xFFFFFFFFL) + synonymsCount;
    }

    public int nodeIdOrSynonym(int node) {
        return childrenCountOrNodeSynonym[node];
    }

    public int[] reset() {
        int[] childrenCount = childrenCountOrNodeSynonym;
        Arrays.fill(childrenCount, 0, nodesCount, 0);
        return childrenCount;
    }
}
