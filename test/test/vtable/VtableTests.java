/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.vtable;

import one.profiler.test.*;
import test.recovery.Numbers;
import test.recovery.Suppliers;

public class VtableTests {

    @Test(mainClass = Numbers.class, jvm = Jvm.HOTSPOT)
    public void vtableStubs(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -i 1ms -F vtable -o collapsed");
        assert out.contains("Numbers.avg;vtable stub;java.lang.Integer_\\[i]");
        assert out.contains("Numbers.avg;vtable stub;java.lang.Long_\\[i]");
    }

    @Test(mainClass = Suppliers.class, jvm = Jvm.HOTSPOT)
    public void itableStubs(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -i 1ms -F comptask,vtable -o collapsed");
        assert out.contains("Suppliers.loop;itable stub;test.recovery.Suppliers[^_]+_\\[i]");
    }
}
