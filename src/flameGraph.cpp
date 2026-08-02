/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <vector>
#include <stdio.h>
#include <string.h>
#include "flameGraph.h"
#include "incbin.h"


// Browsers refuse to draw on canvas larger than 32767 px
const int MAX_CANVAS_HEIGHT = 32767;

INCBIN(FLAMEGRAPH_TEMPLATE, "src/res/flame.html")


class StringUtils {
  public:
    static void replace(std::string& s, char c, const char* replacement, size_t rlen) {
        for (size_t i = 0; (i = s.find(c, i)) != std::string::npos; i += rlen) {
            s.replace(i, 1, replacement, rlen);
        }
    }

    static size_t getCommonPrefix(const std::string& a, const std::string& b) {
        size_t length = a.size() < b.size() ? a.size() : b.size();
        for (size_t i = 0; i < length; i++) {
            if (a[i] != b[i] || a[i] > 127) {
                return i;
            }
        }
        return length;
    }
};


class Format {
  private:
    char _buf[32];

  public:
    const char* thousands(u64 value) {
        char* p = _buf + sizeof(_buf) - 1;
        *p = 0;

        while (value >= 1000) {
            p -= 4;
            p[0] = ',';
            p[1] = '0' + char(value % 1000 / 100);
            p[2] = '0' + char(value % 100 / 10);
            p[3] = '0' + char(value % 10);
            value /= 1000;
        }

        do {
            *--p = '0' + char(value % 10);
        } while ((value /= 10) > 0);

        return p;
    }
};


class Node {
  public:
    u32 _key;
    u32 _order;
    const Trie* _trie;

    Node(u32 key, u32 order, const Trie* trie) : _key(key), _order(order), _trie(trie) {
    }

    static bool orderByName(const Node& a, const Node& b) {
        return a._order < b._order;
    }
};


Trie* FlameGraph::addChild(Trie* f, const char* name, FrameTypeId type, u64 value) {
    size_t len = strlen(name);
    bool has_suffix = len > 4 && name[len - 4] == '_' && name[len - 3] == '[' && name[len - 1] == ']';
    std::string s(name, has_suffix ? len - 4 : len);

    u32 name_index = _cpool[s];
    if (name_index == 0) {
        name_index = _cpool[s] = _cpool.size();
    }

    f->_total += value;

    switch (type) {
        case FRAME_INLINED:
            (f = f->child(name_index, FRAME_JIT_COMPILED))->_inlined += value;
            return f;
        case FRAME_C1_COMPILED:
            (f = f->child(name_index, FRAME_JIT_COMPILED))->_c1_compiled += value;
            return f;
        case FRAME_INTERPRETED:
            (f = f->child(name_index, FRAME_JIT_COMPILED))->_interpreted += value;
            return f;
        default:
            return f->child(name_index, type);
    }
}

void FlameGraph::dump(Writer& out) {
    _name_order = new u32[_cpool.size() + 1]();
    _mintotal = (u64)(_root._total * _minwidth / 100);
    int depth = _root.depth(_mintotal, _name_order);

    const char* tail = FLAMEGRAPH_TEMPLATE;

    tail = printTill(out, tail, "/*height:*/300");
    out << std::min(depth * 16, MAX_CANVAS_HEIGHT);

    tail = printTill(out, tail, "/*title:*/");
    out << _title;

    tail = printTill(out, tail, "/*inverted:*/false");
    // _inverted toggles the layout for reversed stacktraces from icicle to flamegraph
    // and for default stacktraces from flamegraphs to icicle.
    out << (_reverse ^ _inverted ? "true" : "false");

    tail = printTill(out, tail, "/*treeview:*/false");
    out << (_tree ? "true" : "false");

    tail = printTill(out, tail, "/*depth:*/0");
    out << depth;

    tail = printTill(out, tail, "/*cpool:*/");
    printCpool(out);

    tail = printTill(out, tail, "/*frames:*/");
    printFrame(out, FRAME_NATIVE << 28, _root, 0, 0);

    tail = printTill(out, tail, "/*highlight:*/");

    out << tail;

    delete[] _name_order;
}

void FlameGraph::printFrame(Writer& out, u32 key, const Trie& f, int level, u64 x) {
    u32 name_and_type = _name_order[f.nameIndex(key)] << 3 | f.type(key);
    bool has_extra_types = (f._inlined | f._c1_compiled | f._interpreted) &&
                           f._inlined < f._total && f._interpreted < f._total;

    char* p = _buf;
    if (level == _last_level + 1 && x == _last_x) {
        p += snprintf(p, 100, "u(%u", name_and_type);
    } else if (level == _last_level && x == _last_x + _last_total) {
        p += snprintf(p, 100, "n(%u", name_and_type);
    } else {
        p += snprintf(p, 100, "f(%u,%d,%llu", name_and_type, level, x - _last_x);
    }

    if (f._total != _last_total || has_extra_types) {
        p += snprintf(p, 100, ",%llu", f._total);
        if (has_extra_types) {
            p += snprintf(p, 100, ",%llu,%llu,%llu", f._inlined, f._c1_compiled, f._interpreted);
        }
    }

    strcpy(p, ")\n");
    out << _buf;

    _last_level = level;
    _last_x = x;
    _last_total = f._total;

    if (f._children.empty()) {
        return;
    }

    std::vector<Node> children;
    children.reserve(f._children.size());
    for (auto it = f._children.begin(); it != f._children.end(); ++it) {
        children.push_back(Node(it->first, _name_order[f.nameIndex(it->first)], it->second));
    }
    std::sort(children.begin(), children.end(), Node::orderByName);

    x += f._self;
    for (size_t i = 0; i < children.size(); i++) {
        u32 key = children[i]._key;
        const Trie* trie = children[i]._trie;
        if (trie->_total >= _mintotal) {
            printFrame(out, key, *trie, level + 1, x);
        }
        x += trie->_total;
    }
}

void FlameGraph::printCpool(Writer& out) {
    out << "'all'";

    std::string prev;
    u32 index = 0;
    for (std::map<std::string, u32>::const_iterator it = _cpool.begin(); it != _cpool.end(); ++it) {
        if (_name_order[it->second]) {
            _name_order[it->second] = ++index;

            size_t prefix_len = StringUtils::getCommonPrefix(prev, it->first);
            prev = it->first;

            if (prefix_len > 95) prefix_len = 95;
            std::string s(1, (char)(prefix_len + ' '));
            s.append(it->first, prefix_len, std::string::npos);

            StringUtils::replace(s, '\\', "\\\\", 2);
            StringUtils::replace(s, '\'', "\\'", 2);
            out << ",\n'";
            out.write(s.data(), s.size());
            out << "'";
        }
    }

    // Release cpool memory, since frame names are never used beyond this point
    _cpool = std::map<std::string, u32>();
}

const char* FlameGraph::printTill(Writer& out, const char* data, const char* till) {
    const char* pos = strstr(data, till);
    out.write(data, pos - data);
    return pos + strlen(till);
}
