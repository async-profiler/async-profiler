/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.util.Map;

/**
 * Container for collapsed profiling data with optional duration information(JFRs).
 * Used for differential analysis that considers profiling duration.
 */
public class ProfileData {
    public final Map<String, Long> collapsedData;
    public final Double durationSeconds;
    
    public ProfileData(Map<String, Long> collapsedData, Double durationSeconds) {
        this.collapsedData = collapsedData;
        this.durationSeconds = durationSeconds;
    }
}
