/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

public enum Os {
    LINUX,
    MACOS,
    WINDOWS;

    public String getLibExt() {
        switch (this) {
            case LINUX:
                return "so";
            case MACOS:
                return "dylib";
            case WINDOWS:
                return "dll";
            default:
                throw new AssertionError();
        }
    }
}
