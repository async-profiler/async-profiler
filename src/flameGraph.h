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


class Trie {
  private:
    std::map<std::string, Trie> _children;
    u64 _total;
    u64 _self;

  public:
    Trie* addChild(const std::string& key, u64 value) {
        _total += value;
        return &_children[key];
    }

    void addLeaf(u64 value) {
        _total += value;
        _self += value;
    }

    friend class FlameGraph;
};


class FlameGraph {
  private:
    Trie _root;
    char _buf[4096];

    const char* _title;
    int _maxdepth;
    int _imagewidth;
    int _imageheight;
    int _frameheight;
    double _minwidth;
    double _scale;
    double _pct;

    void printHeader(std::ostream& out);
    void printFooter(std::ostream& out);
    double printFrame(std::ostream& out, const std::string& name, const Trie& f, double x, double y);
    int selectFrameColor(std::string& name);

  public:
    FlameGraph(const char* title, int width, int height, double minwidth) :
        _root(),
        _maxdepth(0),
        _title(title),
        _imagewidth(width),
        _frameheight(height),
        _minwidth(minwidth) {
    }

    Trie* root() {
        return &_root;
    }

    void depth(int d) {
        if (d > _maxdepth) _maxdepth = d;
    }

    void dump(std::ostream& out);
};

#endif // _FLAMEGRAPH_H
