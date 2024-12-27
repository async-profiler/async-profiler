/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

public interface FrameFormatter {

    boolean isNativeFrame(byte type);

    String toJavaClassName(byte[] symbol);
}
