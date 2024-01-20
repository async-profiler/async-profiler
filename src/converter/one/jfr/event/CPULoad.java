/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import one.jfr.JfrReader;

public class CPULoad extends Event {
    public final float jvmUser;
    public final float jvmSystem;
    public final float machineTotal;

    public CPULoad(JfrReader jfr) {
        super(jfr.getVarlong(), 0, 0);
        this.jvmUser = jfr.getFloat();
        this.jvmSystem = jfr.getFloat();
        this.machineTotal = jfr.getFloat();
    }
}
