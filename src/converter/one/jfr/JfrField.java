/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr;

import java.util.Map;

public class JfrField extends Element {
    final String name;
    final int type;
    final boolean constantPool;

    JfrField(Map<String, String> attributes) {
        this.name = attributes.get("name");
        this.type = Integer.parseInt(attributes.get("class"));
        this.constantPool = "true".equals(attributes.get("constantPool"));
    }
}
