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
    u64 _interp;
    u64 _inlined;
    u64 _compiled;

    Trie() : _children(), _total(0), _self(0), _interp(0), _inlined(0), _compiled(0) {
    }
    
    Trie* addChild(const std::string& key, u64 value, char type, char next_type) {
        _total += value;
        switch(type) {
        case FRAME_TYPE_INTERPRETED_JAVA: _interp += value; break;
        case FRAME_TYPE_INLINED_JAVA: _inlined += value; break;
        case FRAME_TYPE_COMPILED_JAVA: _compiled += value; break;
        }
        char suffix = type;
        if (next_type == FRAME_TYPE_INTERPRETED_JAVA || next_type == FRAME_TYPE_INLINED_JAVA || next_type == FRAME_TYPE_COMPILED_JAVA) {
          suffix = FRAME_TYPE_UNKNOWN_JAVA;
        } else {
          suffix = next_type;
        }
        std::string access = key + suffix;
        return &_children[access];
    }

    void addLeaf(u64 value, char type) {
        _total += value;
        _self += value;
        switch(type) {
            case FRAME_TYPE_INTERPRETED_JAVA: _interp += value; return;
            case FRAME_TYPE_INLINED_JAVA: _inlined += value; return;
            case FRAME_TYPE_COMPILED_JAVA: _compiled += value; return;
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

class Node {
  public:
    std::string _name;
    const Trie* _trie;

    Node(std::string name, const Trie& trie) : _name(name), _trie(&trie) {
    }

    bool operator<(const Node& other) const {
        return _trie->_total > other._trie->_total;
    }
};

class Palette;


class FlameGraph {
  private:
    Trie _root;
    char _buf[4096];

    const char* _title;
    Counter _counter;
    int _imagewidth;
    int _imageheight;
    int _frameheight;
    double _minwidth;
    double _scale;
    double _pct;
    bool _reverse;
    int _gradient;

    void printHeader(std::ostream& out);
    void printFooter(std::ostream& out);
    int calcPercentage(int *out, u64 total, u64 interp, u64 inlined, u64 compiled);
    double printFrame(std::ostream& out, const std::string& name, const Trie& f, double x, double y);
    void printTreeHeader(std::ostream& out);
    void printTreeFooter(std::ostream& out);
    bool printTreeFrame(std::ostream& out, const Trie& f, int depth);
    const Palette& selectFramePalette(char c);
    const Palette& selectFramePalette(std::string& name, char type);

  public:
    FlameGraph(const char* title, Counter counter, int width, int height, double minwidth, bool reverse) :
        _root(),
        _title(title),
        _counter(counter),
        _imagewidth(width),
        _frameheight(height),
        _minwidth(minwidth),
        _reverse(reverse),
        _gradient(0) {
        _buf[sizeof(_buf) - 1] = 0;
    }

    Trie* root() {
        return &_root;
    }

    void dump(std::ostream& out, bool tree);
};

#endif // _FLAMEGRAPH_H
