/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JFRMETADATA_H
#define _JFRMETADATA_H

#include <string>
#include <map>
#include <vector>
#include <stdio.h>
#include <string.h>


enum JfrType {
    T_METADATA = 0,
    T_CPOOL = 1,

    T_BOOLEAN = 4,
    T_CHAR = 5,
    T_FLOAT = 6,
    T_DOUBLE = 7,
    T_BYTE = 8,
    T_SHORT = 9,
    T_INT = 10,
    T_LONG = 11,

    T_STRING = 20,
    T_CLASS = 21,
    T_THREAD = 22,
    T_CLASS_LOADER = 23,
    T_FRAME_TYPE = 24,
    T_THREAD_STATE = 25,
    T_STACK_TRACE = 26,
    T_STACK_FRAME = 27,
    T_METHOD = 28,
    T_VIRTUAL_SPACE = 29,
    T_PACKAGE = 30,
    T_SYMBOL = 31,
    T_GC_WHEN = 32,
    T_LOG_LEVEL = 33,

    T_EVENT = 100,
    T_EXECUTION_SAMPLE = 101,
    T_ALLOC_IN_NEW_TLAB = 102,
    T_ALLOC_OUTSIDE_TLAB = 103,
    T_MONITOR_ENTER = 104,
    T_THREAD_PARK = 105,
    T_CPU_LOAD = 106,
    T_ACTIVE_RECORDING = 107,
    T_ACTIVE_SETTING = 108,
    T_OS_INFORMATION = 109,
    T_CPU_INFORMATION = 110,
    T_JVM_INFORMATION = 111,
    T_INITIAL_SYSTEM_PROPERTY = 112,
    T_NATIVE_LIBRARY = 113,
    T_GC_HEAP_SUMMARY = 114,
    T_LOG = 115,
    T_WINDOW = 116,
    T_LIVE_OBJECT = 117,
    T_WALL_CLOCK_SAMPLE = 118,

    T_ANNOTATION = 200,
    T_LABEL = 201,
    T_CATEGORY = 202,
    T_CONTENT_TYPE = 203,
    T_FIRST_CONTENT_TYPE = 204,
    T_TIMESTAMP = 204,
    T_TIMESPAN = 205,
    T_DATA_AMOUNT = 206,
    T_MEMORY_ADDRESS = 207,
    T_UNSIGNED = 208,
    T_PERCENTAGE = 209,
    T_LAST_CONTENT_TYPE = 209,
};


class Attribute {
  public:
    int _key;
    int _value;

    Attribute(int key, int value) : _key(key), _value(value) {
    }
};

class Element {
  protected:
    static std::map<std::string, int> _string_map;
    static std::vector<std::string> _strings;

    static int getId(const char* s) {
        std::string str(s);
        int id = _string_map[str];
        if (id == 0) {
            id = _string_map[str] = _string_map.size();
            _strings.push_back(str);
        }
        return id - 1;
    }

  public:
    const int _name;
    std::vector<Attribute> _attributes;
    std::vector<const Element*> _children;

    Element(const char* name) : _name(getId(name)), _attributes(), _children() {
    }

    Element& attribute(const char* key, const char* value) {
        _attributes.push_back(Attribute(getId(key), getId(value)));
        return *this;
    }

    Element& attribute(const char* key, JfrType value) {
        char value_str[16];
        snprintf(value_str, sizeof(value_str), "%d", value);
        return attribute(key, value_str);
    }

    Element& operator<<(const Element& child) {
        _children.push_back(&child);
        return *this;
    }
};

class JfrMetadata : Element {
  private:
    static JfrMetadata _root;

    enum FieldFlags {
        F_CPOOL           = 0x1,
        F_ARRAY           = 0x2,
        F_UNSIGNED        = 0x4,
        F_BYTES           = 0x8,
        F_TIME_TICKS      = 0x10,
        F_TIME_MILLIS     = 0x20,
        F_DURATION_TICKS  = 0x40,
        F_DURATION_NANOS  = 0x80,
        F_DURATION_MILLIS = 0x100,
        F_ADDRESS         = 0x200,
        F_PERCENTAGE      = 0x400,
    };

    static Element& element(const char* name) {
        return *new Element(name);
    }

    static Element& type(const char* name, JfrType id, const char* label = NULL, bool simple = false) {
        Element& e = element("class");
        e.attribute("name", name);
        e.attribute("id", id);
        if (simple) {
            e.attribute("simpleType", "true");
        } else if (id > T_ANNOTATION) {
            e.attribute("superType", "java.lang.annotation.Annotation");
        } else if (id > T_EVENT) {
            e.attribute("superType", "jdk.jfr.Event");
        }
        if (label != NULL) {
            e << annotation(T_LABEL, label);
        }
        if (id >= T_FIRST_CONTENT_TYPE && id <= T_LAST_CONTENT_TYPE) {
            e << annotation(T_CONTENT_TYPE);
        }
        return e;
    }

    static Element& field(const char* name, JfrType type, const char* label = NULL, int flags = 0) {
        Element& e = element("field");
        e.attribute("name", name);
        e.attribute("class", type);
        if (flags & F_CPOOL) {
            e.attribute("constantPool", "true");
        }
        if (flags & F_ARRAY) {
            e.attribute("dimension", "1");
        }
        if (label != NULL) {
            e << annotation(T_LABEL, label);
        }
        if (flags & F_UNSIGNED) {
            e << annotation(T_UNSIGNED);
        } else if (flags & F_BYTES) {
            e << annotation(T_UNSIGNED) << annotation(T_DATA_AMOUNT, "BYTES");
        } else if (flags & F_TIME_TICKS) {
            e << annotation(T_TIMESTAMP, "TICKS");
        } else if (flags & F_TIME_MILLIS) {
            e << annotation(T_TIMESTAMP, "MILLISECONDS_SINCE_EPOCH");
        } else if (flags & F_DURATION_TICKS) {
            e << annotation(T_TIMESPAN, "TICKS");
        } else if (flags & F_DURATION_NANOS) {
            e << annotation(T_TIMESPAN, "NANOSECONDS");
        } else if (flags & F_DURATION_MILLIS) {
            e << annotation(T_TIMESPAN, "MILLISECONDS");
        } else if (flags & F_ADDRESS) {
            e << annotation(T_UNSIGNED) << annotation(T_MEMORY_ADDRESS);
        } else if (flags & F_PERCENTAGE) {
            e << annotation(T_PERCENTAGE);
        }
        return e;
    }

    static Element& annotation(JfrType type, const char* value = NULL) {
        Element& e = element("annotation");
        e.attribute("class", type);
        if (value != NULL) {
            e.attribute("value", value);
        }
        return e;
    }

    static Element& category(const char* value0, const char* value1 = NULL, const char* value2 = NULL) {
        Element& e = annotation(T_CATEGORY);
        e.attribute("value-0", value0);
        if (value1 != NULL) {
            e.attribute("value-1", value1);
            if (value2 != NULL) {
                e.attribute("value-2", value2);
            }
        }
        return e;
    }

  public:
    JfrMetadata();

    static Element* root() {
        return &_root;
    }

    static std::vector<std::string>& strings() {
        return _strings;
    }
};

#endif // _JFRMETADATA_H
