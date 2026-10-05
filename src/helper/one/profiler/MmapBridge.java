/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
package one.profiler;

/** Bootstrap-visible helper for JNA interface mapping. */
public final class MmapBridge {
    private MmapBridge() {}
    public static native long address(Object function);
}
