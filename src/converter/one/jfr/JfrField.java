/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr;

import java.util.Map;

public class JfrField extends Element {
    public final String name;
    public final int type;
    public final boolean constantPool;
    public final int dimension;

    JfrField(Map<String, String> attributes) {
        this.name = attributes.get("name");
        this.type = Integer.parseInt(attributes.get("class"));
        this.constantPool = "true".equals(attributes.get("constantPool"));
        this.dimension = Integer.parseInt(attributes.getOrDefault("dimension", "0"));
    }
}
