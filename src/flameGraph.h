/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FLAMEGRAPH_H
#define _FLAMEGRAPH_H

#include <map>
#include <string>
#include "arch.h"
#include "arguments.h"
#include "vmEntry.h"
#include "writer.h"


class Trie {
  public:
    std::map<u32, Trie> _children;
    u64 _total;
    u64 _self;
    u64 _inlined, _c1_compiled, _interpreted;

    Trie() : _children(), _total(0), _self(0), _inlined(0), _c1_compiled(0), _interpreted(0) {
    }

    FrameTypeId type(u32 key) const {
        if (_inlined * 3 >= _total) {
            return FRAME_INLINED;
        } else if (_c1_compiled * 2 >= _total) {
            return FRAME_C1_COMPILED;
        } else if (_interpreted * 2 >= _total) {
            return FRAME_INTERPRETED;
        } else {
            return (FrameTypeId)(key >> 28);
        }
    }

    u32 nameIndex(u32 key) const {
        return key & ((1 << 28) - 1);
    }

    Trie* child(u32 name_index, FrameTypeId type) {
        return &_children[name_index | type << 28];
    }

    int depth(u64 cutoff, u32* name_order) const {
        int max_depth = 0;
        for (std::map<u32, Trie>::const_iterator it = _children.begin(); it != _children.end(); ++it) {
            if (it->second._total >= cutoff) {
                name_order[nameIndex(it->first)] = 1;
                int d = it->second.depth(cutoff, name_order);
                if (d > max_depth) max_depth = d;
            }
        }
        return max_depth + 1;
    }
};


class FlameGraph {
  private:
    Trie _root;
    std::map<std::string, u32> _cpool;
    u32* _name_order;
    u64 _mintotal;
    char _buf[4096];

    const char* _title;
    Counter _counter;
    double _minwidth;
    bool _reverse;

    int _last_level;
    u64 _last_x;
    u64 _last_total;

    void printFrame(Writer& out, u32 key, const Trie& f, int level, u64 x);
    void printTreeFrame(Writer& out, const Trie& f, int level, const char** names);
    void printCpool(Writer& out);
    const char* printTill(Writer& out, const char* data, const char* till);

  public:
    FlameGraph(const char* title, Counter counter, double minwidth, bool reverse) :
        _root(),
        _cpool(),
        _title(title),
        _counter(counter),
        _minwidth(minwidth),
        _reverse(reverse),
        _last_level(0),
        _last_x(0),
        _last_total(0) {
        _buf[sizeof(_buf) - 1] = 0;
    }

    Trie* root() {
        return &_root;
    }

    Trie* addChild(Trie* f, const char* name, FrameTypeId type, u64 value);

    void dump(Writer& out, bool tree);
};

#endif // _FLAMEGRAPH_H
