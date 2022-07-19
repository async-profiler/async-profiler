/*
 * Copyright 2018 Andrei Pangin
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

#ifndef _FLAMEGRAPH_H
#define _FLAMEGRAPH_H

#include <map>
#include <string>
#include <iostream>
#include "arch.h"
#include "arguments.h"
#include "vmEntry.h"


class Trie {
  public:
    std::map<std::string, Trie> _children;
    u64 _total;
    u64 _self;
    u64 _inlined, _c1_compiled, _interpreted;

    Trie() : _children(), _total(0), _self(0), _inlined(0), _c1_compiled(0), _interpreted(0) {
    }

    Trie* addChild(const std::string& key, u64 value) {
        _total += value;
        return &_children[key];
    }

    void addLeaf(u64 value) {
        _total += value;
        _self += value;
    }

    void addCompilationDetails(int bci, u64 counter) {
        switch (FrameType::decode(bci)) {
            case FRAME_INLINED:     _inlined += counter; break;
            case FRAME_C1_COMPILED: _c1_compiled += counter; break;
            case FRAME_INTERPRETED: _interpreted += counter; break;
            default: break;
        }
    }

    int depth(u64 cutoff) const {
        if (_total < cutoff) {
            return 0;
        }

        int max_depth = 0;
        for (std::map<std::string, Trie>::const_iterator it = _children.begin(); it != _children.end(); ++it) {
            int d = it->second.depth(cutoff);
            if (d > max_depth) max_depth = d;
        }
        return max_depth + 1;
    }
};


class FlameGraph {
  private:
    Trie _root;
    char _buf[4096];
    u64 _mintotal;

    const char* _title;
    Counter _counter;
    double _minwidth;
    bool _reverse;

    void printFrame(std::ostream& out, const std::string& name, const Trie& f, int level, u64 x);
    void printTreeFrame(std::ostream& out, const Trie& f, int level);
    const char* printTill(std::ostream& out, const char* data, const char* till);
    int frameType(std::string& name, const Trie& f);

  public:
    FlameGraph(const char* title, Counter counter, double minwidth, bool reverse) :
        _root(),
        _title(title),
        _counter(counter),
        _minwidth(minwidth),
        _reverse(reverse) {
        _buf[sizeof(_buf) - 1] = 0;
    }

    Trie* root() {
        return &_root;
    }

    void dump(std::ostream& out, bool tree);
};

#endif // _FLAMEGRAPH_H
