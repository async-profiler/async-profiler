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
#include "flameGraph.h"


// Browsers refuse to draw on canvas larger than 32767 px
const int MAX_CANVAS_HEIGHT = 32767;

static const char FLAMEGRAPH_HEADER[] =
    "<!DOCTYPE html>\n"
    "<html lang='en'>\n"
    "<head>\n"
    "<meta charset='utf-8'>\n"
    "<style>\n"
    "\tbody {margin: 0; padding: 10px; background-color: #ffffff}\n"
    "\th1 {margin: 5px 0 0 0; font-size: 18px; font-weight: normal; text-align: center}\n"
    "\theader {margin: -24px 0 5px 0; line-height: 24px}\n"
    "\tbutton {font: 12px sans-serif; cursor: pointer}\n"
    "\tp {margin: 5px 0 5px 0}\n"
    "\ta {color: #0366d6}\n"
    "\t#hl {position: absolute; display: none; overflow: hidden; white-space: nowrap; pointer-events: none; background-color: #ffffe0; outline: 1px solid #ffc000; height: 15px}\n"
    "\t#hl span {padding: 0 3px 0 3px}\n"
    "\t#status {overflow: hidden; white-space: nowrap}\n"
    "\t#match {overflow: hidden; white-space: nowrap; display: none; float: right; text-align: right}\n"
    "\t#reset {cursor: pointer}\n"
    "</style>\n"
    "</head>\n"
    "<body style='font: 12px Verdana, sans-serif'>\n"
    "<h1>%s</h1>\n"
    "<header style='text-align: left'><button id='reverse' title='Reverse'>&#x1f53b;</button>&nbsp;&nbsp;<button id='search' title='Search'>&#x1f50d;</button></header>\n"
    "<header style='text-align: right'>Produced by <a href='https://github.com/jvm-profiling-tools/async-profiler'>async-profiler</a></header>\n"
    "<canvas id='canvas' style='width: 100%%; height: %dpx'></canvas>\n"
    "<div id='hl'><span></span></div>\n"
    "<p id='match'>Matched: <span id='matchval'></span> <span id='reset' title='Clear'>&#x274c;</span></p>\n"
    "<p id='status'>&nbsp;</p>\n"
    "<script>\n"
    "\t// Copyright 2020 Andrei Pangin\n"
    "\t// Licensed under the Apache License, Version 2.0.\n"
    "\t'use strict';\n"
    "\tvar root, rootLevel, px, pattern;\n"
    "\tvar reverse = %s;\n"
    "\tconst levels = Array(%d);\n"
    "\tfor (let h = 0; h < levels.length; h++) {\n"
    "\t\tlevels[h] = [];\n"
    "\t}\n"
    "\n"
    "\tconst canvas = document.getElementById('canvas');\n"
    "\tconst c = canvas.getContext('2d');\n"
    "\tconst hl = document.getElementById('hl');\n"
    "\tconst status = document.getElementById('status');\n"
    "\n"
    "\tconst canvasWidth = canvas.offsetWidth;\n"
    "\tconst canvasHeight = canvas.offsetHeight;\n"
    "\tcanvas.style.width = canvasWidth + 'px';\n"
    "\tcanvas.width = canvasWidth * (devicePixelRatio || 1);\n"
    "\tcanvas.height = canvasHeight * (devicePixelRatio || 1);\n"
    "\tif (devicePixelRatio) c.scale(devicePixelRatio, devicePixelRatio);\n"
    "\tc.font = document.body.style.font;\n"
    "\n"
    "\tconst palette = [\n"
    "\t\t[0x50e150, 30, 30, 30],\n"
    "\t\t[0x50bebe, 30, 30, 30],\n"
    "\t\t[0xe17d00, 30, 30,  0],\n"
    "\t\t[0xc8c83c, 30, 30, 10],\n"
    "\t\t[0xe15a5a, 30, 40, 40],\n"
    "\t];\n"
    "\n"
    "\tfunction getColor(p) {\n"
    "\t\tconst v = Math.random();\n"
    "\t\treturn '#' + (p[0] + ((p[1] * v) << 16 | (p[2] * v) << 8 | (p[3] * v))).toString(16);\n"
    "\t}\n"
    "\n"
    "\tfunction f(level, left, width, type, title) {\n"
    "\t\tlevels[level].push({left: left, width: width, color: getColor(palette[type]), title: title});\n"
    "\t}\n"
    "\n"
    "\tfunction samples(n) {\n"
    "\t\treturn n === 1 ? '1 sample' : n.toString().replace(/\\B(?=(\\d{3})+(?!\\d))/g, ',') + ' samples';\n"
    "\t}\n"
    "\n"
    "\tfunction pct(a, b) {\n"
    "\t\treturn a >= b ? '100' : (100 * a / b).toFixed(2);\n"
    "\t}\n"
    "\n"
    "\tfunction findFrame(frames, x) {\n"
    "\t\tlet left = 0;\n"
    "\t\tlet right = frames.length - 1;\n"
    "\n"
    "\t\twhile (left <= right) {\n"
    "\t\t\tconst mid = (left + right) >>> 1;\n"
    "\t\t\tconst f = frames[mid];\n"
    "\n"
    "\t\t\tif (f.left > x) {\n"
    "\t\t\t\tright = mid - 1;\n"
    "\t\t\t} else if (f.left + f.width <= x) {\n"
    "\t\t\t\tleft = mid + 1;\n"
    "\t\t\t} else {\n"
    "\t\t\t\treturn f;\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\n"
    "\t\tif (frames[left] && (frames[left].left - x) * px < 0.5) return frames[left];\n"
    "\t\tif (frames[right] && (x - (frames[right].left + frames[right].width)) * px < 0.5) return frames[right];\n"
    "\n"
    "\t\treturn null;\n"
    "\t}\n"
    "\n"
    "\tfunction search(r) {\n"
    "\t\tif (r && (r = prompt('Enter regexp to search:', '')) === null) {\n"
    "\t\t\treturn;\n"
    "\t\t}\n"
    "\n"
    "\t\tpattern = r ? RegExp(r) : undefined;\n"
    "\t\tconst matched = render(root, rootLevel);\n"
    "\t\tdocument.getElementById('matchval').textContent = pct(matched, root.width) + '%%';\n"
    "\t\tdocument.getElementById('match').style.display = r ? 'inherit' : 'none';\n"
    "\t}\n"
    "\n"
    "\tfunction render(newRoot, newLevel) {\n"
    "\t\tif (root) {\n"
    "\t\t\tc.fillStyle = '#ffffff';\n"
    "\t\t\tc.fillRect(0, 0, canvasWidth, canvasHeight);\n"
    "\t\t}\n"
    "\n"
    "\t\troot = newRoot || levels[0][0];\n"
    "\t\trootLevel = newLevel || 0;\n"
    "\t\tpx = canvasWidth / root.width;\n"
    "\n"
    "\t\tconst x0 = root.left;\n"
    "\t\tconst x1 = x0 + root.width;\n"
    "\t\tconst marked = [];\n"
    "\n"
    "\t\tfunction mark(f) {\n"
    "\t\t\treturn marked[f.left] >= f.width || (marked[f.left] = f.width);\n"
    "\t\t}\n"
    "\n"
    "\t\tfunction totalMarked() {\n"
    "\t\t\tlet total = 0;\n"
    "\t\t\tlet left = 0;\n"
    "\t\t\tfor (let x in marked) {\n"
    "\t\t\t\tif (+x >= left) {\n"
    "\t\t\t\t\ttotal += marked[x];\n"
    "\t\t\t\t\tleft = +x + marked[x];\n"
    "\t\t\t\t}\n"
    "\t\t\t}\n"
    "\t\t\treturn total;\n"
    "\t\t}\n"
    "\n"
    "\t\tfunction drawFrame(f, y, alpha) {\n"
    "\t\t\tif (f.left < x1 && f.left + f.width > x0) {\n"
    "\t\t\t\tc.fillStyle = pattern && f.title.match(pattern) && mark(f) ? '#ee00ee' : f.color;\n"
    "\t\t\t\tc.fillRect((f.left - x0) * px, y, f.width * px, 15);\n"
    "\n"
    "\t\t\t\tif (f.width * px >= 21) {\n"
    "\t\t\t\t\tconst chars = Math.floor(f.width * px / 7);\n"
    "\t\t\t\t\tconst title = f.title.length <= chars ? f.title : f.title.substring(0, chars - 2) + '..';\n"
    "\t\t\t\t\tc.fillStyle = '#000000';\n"
    "\t\t\t\t\tc.fillText(title, Math.max(f.left - x0, 0) * px + 3, y + 12, f.width * px - 6);\n"
    "\t\t\t\t}\n"
    "\n"
    "\t\t\t\tif (alpha) {\n"
    "\t\t\t\t\tc.fillStyle = 'rgba(255, 255, 255, 0.5)';\n"
    "\t\t\t\t\tc.fillRect((f.left - x0) * px, y, f.width * px, 15);\n"
    "\t\t\t\t}\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\n"
    "\t\tfor (let h = 0; h < levels.length; h++) {\n"
    "\t\t\tconst y = reverse ? h * 16 : canvasHeight - (h + 1) * 16;\n"
    "\t\t\tconst frames = levels[h];\n"
    "\t\t\tfor (let i = 0; i < frames.length; i++) {\n"
    "\t\t\t\tdrawFrame(frames[i], y, h < rootLevel);\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\n"
    "\t\treturn totalMarked();\n"
    "\t}\n"
    "\n"
    "\tcanvas.onmousemove = function() {\n"
    "\t\tconst h = Math.floor((reverse ? event.offsetY : (canvasHeight - event.offsetY)) / 16);\n"
    "\t\tif (h >= 0 && h < levels.length) {\n"
    "\t\t\tconst f = findFrame(levels[h], event.offsetX / px + root.left);\n"
    "\t\t\tif (f) {\n"
    "\t\t\t\thl.style.left = (Math.max(f.left - root.left, 0) * px + canvas.offsetLeft) + 'px';\n"
    "\t\t\t\thl.style.width = (Math.min(f.width, root.width) * px) + 'px';\n"
    "\t\t\t\thl.style.top = ((reverse ? h * 16 : canvasHeight - (h + 1) * 16) + canvas.offsetTop) + 'px';\n"
    "\t\t\t\thl.firstChild.textContent = f.title;\n"
    "\t\t\t\thl.style.display = 'block';\n"
    "\t\t\t\tcanvas.title = f.title + '\\n(' + samples(f.width) + ', ' + pct(f.width, levels[0][0].width) + '%%)';\n"
    "\t\t\t\tcanvas.style.cursor = 'pointer';\n"
    "\t\t\t\tcanvas.onclick = function() {\n"
    "\t\t\t\t\tif (f != root) {\n"
    "\t\t\t\t\t\trender(f, h);\n"
    "\t\t\t\t\t\tcanvas.onmousemove();\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t};\n"
    "\t\t\t\tstatus.textContent = 'Function: ' + canvas.title;\n"
    "\t\t\t\treturn;\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\tcanvas.onmouseout();\n"
    "\t}\n"
    "\n"
    "\tcanvas.onmouseout = function() {\n"
    "\t\thl.style.display = 'none';\n"
    "\t\tstatus.textContent = '\\xa0';\n"
    "\t\tcanvas.title = '';\n"
    "\t\tcanvas.style.cursor = '';\n"
    "\t\tcanvas.onclick = '';\n"
    "\t}\n"
    "\n"
    "\tdocument.getElementById('reverse').onclick = function() {\n"
    "\t\treverse = !reverse;\n"
    "\t\trender();\n"
    "\t}\n"
    "\n"
    "\tdocument.getElementById('search').onclick = function() {\n"
    "\t\tsearch(true);\n"
    "\t}\n"
    "\n"
    "\tdocument.getElementById('reset').onclick = function() {\n"
    "\t\tsearch(false);\n"
    "\t}\n"
    "\n"
    "\twindow.onkeydown = function() {\n"
    "\t\tif (event.ctrlKey && event.keyCode === 70) {\n"
    "\t\t\tevent.preventDefault();\n"
    "\t\t\tsearch(true);\n"
    "\t\t} else if (event.keyCode === 27) {\n"
    "\t\t\tsearch(false);\n"
    "\t\t}\n"
    "\t}\n";

static const char FLAMEGRAPH_FOOTER[] =
    "render();\n"
    "</script></body></html>\n";


static const char TREE_HEADER[] =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "<title>Tree view</title>\n"
    "<meta charset=\"utf-8\"/>\n"
    "<style>\n"
    "body {\n"
    "    font-family: Arial;\n"
    "}\n"
    "ul.tree li {\n"
    "    list-style-type: none;\n"
    "    position: relative;\n"
    "}\n"
    "ul.tree ul {\n"
    "    margin-left: 20px; padding-left: 0;\n"
    "}\n"
    "ul.tree li ul {\n"
    "    display: none;\n"
    "}\n"
    "ul.tree li.open > ul {\n"
    "    display: block;\n"
    "}\n"
    "ul.tree li div:before {\n"
    "    height: 1em;\n"
    "    padding:0 .1em;\n"
    "    font-size: .8em;\n"
    "    display: block;\n"
    "    position: absolute;\n"
    "    left: -1.3em;\n"
    "    top: .2em;\n"
    "}\n"
    "ul.tree li > div:not(:nth-last-child(2)):before {\n"
    "    content: '+';\n"
    "}\n"
    "ul.tree li.open > div:not(:nth-last-child(2)):before {\n"
    "    content: '-';\n"
    "}\n"
    ".sc {\n"
    "    text-decoration: underline;\n"
    "    text-decoration-color: black;\n"
    "    font-weight: bold;\n"
    "    background-color: #D9D9D9;\n"
    "}\n"
    ".t0 {\n"
    "    color: #32c832;\n"
    "}\n"
    ".t1 {\n"
    "    color: #32a5a5;\n"
    "}\n"
    ".t2 {\n"
    "    color: #be5a00;\n"
    "}\n"
    ".t3 {\n"
    "    color: #afaf32;\n"
    "}\n"
    ".t4 {\n"
    "    color: #c83232;\n"
    "}\n"
    "ul.tree li > div {\n"
    "    display: inline;\n"
    "    cursor: pointer;\n"
    "    color: black;\n"
    "    text-decoration: none;\n"
    "}\n"
    "</style>\n"
    "<script>\n"
    "function treeView(opt) {\n"
    "    var tree = document.querySelectorAll('ul.tree div:not(:last-child)');\n"
    "    for(var i = 0; i < tree.length; i++){\n"
    "        var parent = tree[i].parentElement;\n"
    "        var classList = parent.classList;\n"
    "        if(opt == 0) {\n"
    "            classList.add('open');\n"
    "        } else {\n"
    "            classList.remove('open');\n"
    "        }\n"
    "    }\n"
    "}\n"
    "function openParent(p,t) {\n"
    "    if(p.parentElement.classList.contains(\"tree\")) {\n"
    "        return;\n"
    "    }\n"
    "    p.parentElement.classList.add('open');\n"
    "    openParent(p.parentElement,t);\n"
    "}\n"
    "function search() {\n"
    "    var tree = document.querySelectorAll('ul.tree span');\n"
    "    var check = document.getElementById('check');\n"
    "    for(var i = 0; i < tree.length; i++){\n"
    "        tree[i].classList.remove('sc');\n"
    "        if(tree[i].innerHTML.includes(document.getElementById(\"search\").value)) {\n"
    "            tree[i].classList.add('sc');\n"
    "            openParent(tree[i].parentElement,tree);\n"
    "        }\n"
    "    }\n"
    "}\n"
    "function openUL(n) {\n"
    "    var children = n.children;\n"
    "    if(children.length == 1) {\n"
    "        openNode(children[0]);\n"
    "    }\n"
    "}\n"
    "function openNode(n) {\n"
    "    var children = n.children;\n"
    "    for(var i = 0; i < children.length; i++){\n"
    "        if(children[i].nodeName == 'UL') {\n"
    "            n.classList.add('open');\n"
    "            openUL(children[i]);\n"
    "        }\n"
    "    }\n"
    "}\n"
    "function addClickActions() {\n"
    "var tree = document.querySelectorAll('ul.tree div:not(:last-child)');\n"
    "for(var i = 0; i < tree.length; i++){\n"
    "    tree[i].addEventListener('click', function(e) {\n"
    "        var parent = e.target.parentElement;\n"
    "        var classList = parent.classList;\n"
    "        if(classList.contains(\"open\")) {\n"
    "            classList.remove('open');\n"
    "            var opensubs = parent.querySelectorAll(':scope .open');\n"
    "            for(var i = 0; i < opensubs.length; i++){\n"
    "                opensubs[i].classList.remove('open');\n"
    "            }\n"
    "        } else {\n"
    "            if(e.altKey) {\n"
    "                classList.add('open');\n"
    "                var opensubs = parent.querySelectorAll('li');\n"
    "                for(var i = 0; i < opensubs.length; i++){\n"
    "                    opensubs[i].classList.add('open');\n"
    "                }\n"
    "            } else {\n"
    "                openNode(parent);\n"
    "            }\n"
    "        }\n"
    "    });\n"
    "}\n"
    "}\n"
    "</script>\n"
    "</head>\n"
    "<body>\n"
    "<div style=\"padding-left: 25px;\">%s view, total %s: %s </div>\n"
    "<div style=\"padding-left: 25px;\"><button type='button' onclick='treeView(0)'>++</button><button type='button' onclick='treeView(1)'>--</button>\n"
    "<input type='text' id='search' value='' size='35' onkeypress=\"if(event.keyCode == 13) document.getElementById('searchBtn').click()\">\n"
    "<button type='button' id='searchBtn' onclick='search()'>search</button></div>\n"
    "<ul class=\"tree\">\n";

static const char TREE_FOOTER[] = 
    "<script>\n"
    "addClickActions();\n"
    "</script>\n"
    "</ul>\n"
    "</body>\n"
    "</html>\n";


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
        char buf[sizeof(TREE_HEADER) + 256];
        snprintf(buf, sizeof(buf) - 1, TREE_HEADER,
                 _reverse ? "Backtrace" : "Call tree",
                 _counter ==  COUNTER_SAMPLES ? "samples" : "counter",
                 Format().thousands(_root._total));
        out << buf;

        printTreeFrame(out, _root, 0);

        out << TREE_FOOTER;
    } else {
        char buf[sizeof(FLAMEGRAPH_HEADER) + 256];
        snprintf(buf, sizeof(buf) - 1, FLAMEGRAPH_HEADER, _title,
                 std::min(depth * 16, MAX_CANVAS_HEIGHT), _reverse ? "true" : "false", depth);
        out << buf;

        printFrame(out, "all", _root, 0, 0);

        out << FLAMEGRAPH_FOOTER;
    }
}

void FlameGraph::printFrame(std::ostream& out, const std::string& name, const Trie& f, int level, u64 x) {
    std::string name_copy = name;
    int type = frameType(name_copy);
    StringUtils::replace(name_copy, '\'', "\\'", 2);

    snprintf(_buf, sizeof(_buf) - 1, "f(%d,%llu,%llu,%d,'%s')\n", level, x, f._total, type, name_copy.c_str());
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

        int type = frameType(name);
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

int FlameGraph::frameType(std::string& name) {
    if (StringUtils::endsWith(name, "_[j]", 4)) {
        // Java compiled frame
        name = name.substr(0, name.length() - 4);
        return 0;
    } else if (StringUtils::endsWith(name, "_[i]", 4)) {
        // Java inlined frame
        name = name.substr(0, name.length() - 4);
        return 1;
    } else if (StringUtils::endsWith(name, "_[k]", 4)) {
        // Kernel function
        name = name.substr(0, name.length() - 4);
        return 2;
    } else if (name.find("::") != std::string::npos || name.compare(0, 2, "-[") == 0 || name.compare(0, 2, "+[") == 0) {
        // C++ function or Objective C method
        return 3;
    } else if ((int)name.find('/') > 0 && name[0] != '[' || ((int)name.find('.') > 0 && name[0] >= 'A' && name[0] <= 'Z')) {
        // Java regular method
        return 0;
    } else {
        // Other native code
        return 4;
    }
}
