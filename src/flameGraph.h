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


class Trie {
  public:
    std::map<std::string, Trie> _children;
    u64 _total;
    u64 _self;

    Trie() : _children(), _total(0), _self(0) {
    }
    
    Trie* addChild(const std::string& key, u64 value) {
        _total += value;
        return &_children[key];
    }

    void addLeaf(u64 value) {
        _total += value;
        _self += value;
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

    void printHeader(std::ostream& out);
    void printFooter(std::ostream& out);
    double printFrame(std::ostream& out, const std::string& name, const Trie& f, double x, double y);
    void printTreeHeader(std::ostream& out);
    void printTreeFooter(std::ostream& out);
    bool printTreeFrame(std::ostream& out, const Trie& f, int depth);
    const Palette& selectFramePalette(std::string& name);

  public:
    FlameGraph(const char* title, Counter counter, int width, int height, double minwidth, bool reverse) :
        _root(),
        _title(title),
        _counter(counter),
        _imagewidth(width),
        _frameheight(height),
        _minwidth(minwidth),
        _reverse(reverse) {
    }

    Trie* root() {
        return &_root;
    }

    void dump(std::ostream& out, bool tree);
};

#endif // _FLAMEGRAPH_H
