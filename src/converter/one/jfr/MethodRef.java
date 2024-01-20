/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr;

public class MethodRef {
    public final long cls;
    public final long name;
    public final long sig;

    public MethodRef(long cls, long name, long sig) {
        this.cls = cls;
        this.name = name;
        this.sig = sig;
    }
}
