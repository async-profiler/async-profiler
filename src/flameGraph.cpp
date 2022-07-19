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

#include <algorithm>
#include <vector>
#include <stdio.h>
#include <string.h>
#include "flameGraph.h"
#include "incbin.h"
#include "vmEntry.h"


// Browsers refuse to draw on canvas larger than 32767 px
const int MAX_CANVAS_HEIGHT = 32767;

INCBIN(FLAMEGRAPH_TEMPLATE, "flame.html")
INCBIN(TREE_TEMPLATE, "tree.html")


class StringUtils {
  public:
    static bool endsWith(const std::string& s, const char* suffix, size_t suffixlen) {
        size_t len = s.length();
        return len >= suffixlen && s.compare(len - suffixlen, suffixlen, suffix) == 0; 
    }

    static void replace(std::string& s, char c, const char* replacement, size_t rlen) {
        for (size_t i = 0; (i = s.find(c, i)) != std::string::npos; i += rlen) {
            s.replace(i, 1, replacement, rlen);
        }
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
    std::string _name;
    const Trie* _trie;

    Node(const std::string& name, const Trie& trie) : _name(name), _trie(&trie) {
    }

    bool operator<(const Node& other) const {
        return _trie->_total > other._trie->_total;
    }
};


void FlameGraph::dump(std::ostream& out, bool tree) {
    _mintotal = _minwidth == 0 && tree ? _root._total / 1000 : (u64)(_root._total * _minwidth / 100);
    int depth = _root.depth(_mintotal);

    if (tree) {
        const char* tail = TREE_TEMPLATE;

        tail = printTill(out, tail, "/*title:*/");
        out << (_reverse ? "Backtrace" : "Call tree");

        tail = printTill(out, tail, "/*type:*/");
        out << (_counter == COUNTER_SAMPLES ? "samples" : "counter");

        tail = printTill(out, tail, "/*count:*/");
        out << Format().thousands(_root._total);

        tail = printTill(out, tail, "/*tree:*/");

        printTreeFrame(out, _root, 0);

        out << tail;
    } else {
        const char* tail = FLAMEGRAPH_TEMPLATE;

        tail = printTill(out, tail, "/*height:*/300");
        out << std::min(depth * 16, MAX_CANVAS_HEIGHT);

        tail = printTill(out, tail, "/*title:*/");
        out << _title;

        tail = printTill(out, tail, "/*reverse:*/false");
        out << (_reverse ? "true" : "false");

        tail = printTill(out, tail, "/*depth:*/0");
        out << depth;

        tail = printTill(out, tail, "/*frames:*/");

        printFrame(out, "all", _root, 0, 0);

        tail = printTill(out, tail, "/*highlight:*/");

        out << tail;
    }
}

void FlameGraph::printFrame(std::ostream& out, const std::string& name, const Trie& f, int level, u64 x) {
    std::string name_copy = name;
    int type = frameType(name_copy, f);
    StringUtils::replace(name_copy, '\'', "\\'", 2);

    if (f._inlined | f._c1_compiled | f._interpreted) {
        snprintf(_buf, sizeof(_buf) - 1, "f(%d,%llu,%llu,%d,'%s',%llu,%llu,%llu)\n",
                 level, x, f._total, type, name_copy.c_str(), f._inlined, f._c1_compiled, f._interpreted);
    } else {
        snprintf(_buf, sizeof(_buf) - 1, "f(%d,%llu,%llu,%d,'%s')\n",
                 level, x, f._total, type, name_copy.c_str());
    }
    out << _buf;

    x += f._self;
    for (std::map<std::string, Trie>::const_iterator it = f._children.begin(); it != f._children.end(); ++it) {
        if (it->second._total >= _mintotal) {
            printFrame(out, it->first, it->second, level + 1, x);
        }
        x += it->second._total;
    }
}

void FlameGraph::printTreeFrame(std::ostream& out, const Trie& f, int level) {
    std::vector<Node> subnodes;
    for (std::map<std::string, Trie>::const_iterator it = f._children.begin(); it != f._children.end(); ++it) {
        subnodes.push_back(Node(it->first, it->second));
    }
    std::sort(subnodes.begin(), subnodes.end());

    double pct = 100.0 / _root._total;
    for (size_t i = 0; i < subnodes.size(); i++) {
        std::string name = subnodes[i]._name;
        const Trie* trie = subnodes[i]._trie;

        int type = frameType(name, f);
        StringUtils::replace(name, '&', "&amp;", 5);
        StringUtils::replace(name, '<', "&lt;", 4);
        StringUtils::replace(name, '>', "&gt;", 4);

        if (_reverse) {
            snprintf(_buf, sizeof(_buf) - 1,
                     "<li><div>[%d] %.2f%% %s</div><span class=\"t%d\"> %s</span>\n",
                     level,
                     trie->_total * pct, Format().thousands(trie->_total),
                     type, name.c_str());
        } else {
            snprintf(_buf, sizeof(_buf) - 1,
                     "<li><div>[%d] %.2f%% %s self: %.2f%% %s</div><span class=\"t%d\"> %s</span>\n",
                     level,
                     trie->_total * pct, Format().thousands(trie->_total),
                     trie->_self * pct, Format().thousands(trie->_self),
                     type, name.c_str());
        }
        out << _buf;

        if (trie->_children.size() > 0) {
            out << "<ul>\n";
            if (trie->_total >= _mintotal) {
                printTreeFrame(out, *trie, level + 1);
            } else {
                out << "<li>...\n";
            }
            out << "</ul>\n";
        }
    }
}

const char* FlameGraph::printTill(std::ostream& out, const char* data, const char* till) {
    const char* pos = strstr(data, till);
    out.write(data, pos - data);
    return pos + strlen(till);
}

// TODO: Reuse frame type embedded in ASGCT_CallFrame
int FlameGraph::frameType(std::string& name, const Trie& f) {
    if (f._inlined * 3 >= f._total) {
        return FRAME_INLINED;
    } else if (f._c1_compiled * 2 >= f._total) {
        return FRAME_C1_COMPILED;
    } else if (f._interpreted * 2 >= f._total) {
        return FRAME_INTERPRETED;
    }

    if (StringUtils::endsWith(name, "_[j]", 4)) {
        name = name.substr(0, name.length() - 4);
        return FRAME_JIT_COMPILED;
    } else if (StringUtils::endsWith(name, "_[i]", 4)) {
        name = name.substr(0, name.length() - 4);
        return FRAME_INLINED;
    } else if (StringUtils::endsWith(name, "_[k]", 4)) {
        name = name.substr(0, name.length() - 4);
        return FRAME_KERNEL;
    } else if (name.find("::") != std::string::npos || name.compare(0, 2, "-[") == 0 || name.compare(0, 2, "+[") == 0) {
        return FRAME_CPP;
    } else if (((int)name.find('/') > 0 && name[0] != '[')
            || ((int)name.find('.') > 0 && name[0] >= 'A' && name[0] <= 'Z')) {
        return FRAME_JIT_COMPILED;
    } else {
        return FRAME_NATIVE;
    }
}
