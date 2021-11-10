/*
 * Copyright 2020 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package one.jfr;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

class JfrClass extends Element {
    final int id;
    final String name;
    final List<JfrField> fields;

    JfrClass(Map<String, String> attributes) {
        this.id = Integer.parseInt(attributes.get("id"));
        this.name = attributes.get("name");
        this.fields = new ArrayList<>(2);
    }

    @Override
    void addChild(Element e) {
        if (e instanceof JfrField) {
            fields.add((JfrField) e);
        }
    }

    JfrField field(String name) {
        for (JfrField field : fields) {
            if (field.name.equals(name)) {
                return field;
            }
        }
        return null;
    }
}
