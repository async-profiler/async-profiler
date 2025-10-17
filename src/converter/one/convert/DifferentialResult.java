/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.util.Map;

public class DifferentialResult {
    public final Map<String, String> stackData;
    public final Map<String, Long> functionDeltas;
    public final long maxDelta;
    
    public DifferentialResult(Map<String, String> stackData, Map<String, Long> functionDeltas, long maxDelta) {
        this.stackData = stackData;
        this.functionDeltas = functionDeltas;
        this.maxDelta = maxDelta;
    }
}
