/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

public enum SkipReason {
    Disabled,
    ArchMismatch,
    OsMismatch,
    JvmMismatch,
    JvmVersionMismatch,
    SkipByClassName,
    SkipByMethodName,
}
