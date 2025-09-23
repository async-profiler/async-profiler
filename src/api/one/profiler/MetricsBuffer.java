/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package one.profiler;

import java.nio.ByteBuffer;

public class MetricsBuffer {
    private static volatile ByteBuffer BUFFER = null;
    
    private static ByteBuffer getBuffer() {
        if (BUFFER == null) {
            synchronized (MetricsBuffer.class) {
                if (BUFFER == null) {
                    try {
                        BUFFER = AsyncProfiler.getMetricsBuffer();
                    } catch (UnsatisfiedLinkError e) {
                        return null;
                    }
                }
            }
        }
        return BUFFER;
    }
    
    public static void getMetrics(int[] metrics) {
        ByteBuffer buffer = getBuffer();
        if (buffer == null) return;
        
        buffer.position(0);
        for (int i = 0; i < 5; i++) {
            metrics[i] = buffer.getInt();
            System.out.println(metrics[i]);
        }
    }
}

