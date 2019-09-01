/*
 * Copyright 2018 Andrei Pangin
 *
 * This is a specialized C++ port of the FlameGraph script available at
 * https://github.com/brendangregg/FlameGraph/blob/master/flamegraph.pl
 *
 * Copyright 2016 Netflix, Inc.
 * Copyright 2011 Joyent, Inc.  All rights reserved.
 * Copyright 2011 Brendan Gregg.  All rights reserved.
 *
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at docs/cddl1.txt or
 * http://opensource.org/licenses/CDDL-1.0.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at docs/cddl1.txt.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

#include <iomanip>
#include <vector>
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "flameGraph.h"


static const char SVG_HEADER[] =
    "<?xml version=\"1.0\" standalone=\"no\"?>\n"
    "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n"
    "<svg version=\"1.1\" width=\"%d\" height=\"%d\" onload=\"init(evt)\" viewBox=\"0 0 %d %d\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n"
    "<style type=\"text/css\">\n"
    "\ttext { font-family:Verdana; font-size:12px; fill:rgb(0,0,0); }\n"
    "\t#search { opacity:0.1; cursor:pointer; }\n"
    "\t#search:hover, #search.show { opacity:1; }\n"
    "\t#subtitle { text-anchor:middle; font-color:rgb(160,160,160); }\n"
    "\t#title { text-anchor:middle; font-size:17px}\n"
    "\t#unzoom { cursor:pointer; }\n"
    "\t#frames > *:hover { stroke:black; stroke-width:0.5; cursor:pointer; }\n"
    "\t.hide { display:none; }\n"
    "\t.parent { opacity:0.5; }\n"
    "</style>\n"
    "<script type=\"text/ecmascript\">\n"
    "<![CDATA[\n"
    "\t\"use strict\";\n"
    "\tvar details, searchbtn, unzoombtn, matchedtxt, svg, searching;\n"
    "\tfunction init(evt) {\n"
    "\t\tdetails = document.getElementById(\"details\").firstChild;\n"
    "\t\tsearchbtn = document.getElementById(\"search\");\n"
    "\t\tunzoombtn = document.getElementById(\"unzoom\");\n"
    "\t\tmatchedtxt = document.getElementById(\"matched\");\n"
    "\t\tsvg = document.getElementsByTagName(\"svg\")[0];\n"
    "\t\tsearching = 0;\n"
    "\t}\n"
    "\n"
    "\twindow.addEventListener(\"click\", function(e) {\n"
    "\t\tvar target = find_group(e.target);\n"
    "\t\tif (target) {\n"
    "\t\t\tif (target.nodeName == \"a\") {\n"
    "\t\t\t\tif (e.ctrlKey === false) return;\n"
    "\t\t\t\te.preventDefault();\n"
    "\t\t\t}\n"
    "\t\t\tif (target.classList.contains(\"parent\")) unzoom();\n"
    "\t\t\tzoom(target);\n"
    "\t\t}\n"
    "\t\telse if (e.target.id == \"unzoom\") unzoom();\n"
    "\t\telse if (e.target.id == \"search\") search_prompt();\n"
    "\t}, false)\n"
    "\n"
    "\t// mouse-over for info\n"
    "\t// show\n"
    "\twindow.addEventListener(\"mouseover\", function(e) {\n"
    "\t\tvar target = find_group(e.target);\n"
    "\t\tif (target) details.nodeValue = \"Function: \" + g_to_text(target);\n"
    "\t}, false)\n"
    "\n"
    "\t// clear\n"
    "\twindow.addEventListener(\"mouseout\", function(e) {\n"
    "\t\tvar target = find_group(e.target);\n"
    "\t\tif (target) details.nodeValue = ' ';\n"
    "\t}, false)\n"
    "\n"
    "\t// ctrl-F for search\n"
    "\twindow.addEventListener(\"keydown\",function (e) {\n"
    "\t\tif (e.keyCode === 114 || (e.ctrlKey && e.keyCode === 70)) {\n"
    "\t\t\te.preventDefault();\n"
    "\t\t\tsearch_prompt();\n"
    "\t\t}\n"
    "\t}, false)\n"
    "\n"
    "\t// functions\n"
    "\tfunction find_child(node, selector) {\n"
    "\t\tvar children = node.querySelectorAll(selector);\n"
    "\t\tif (children.length) return children[0];\n"
    "\t\treturn;\n"
    "\t}\n"
    "\tfunction find_group(node) {\n"
    "\t\tvar parent = node.parentElement;\n"
    "\t\tif (!parent) return;\n"
    "\t\tif (parent.id == \"frames\") return node;\n"
    "\t\treturn find_group(parent);\n"
    "\t}\n"
    "\tfunction orig_save(e, attr, val) {\n"
    "\t\tif (e.attributes[\"_orig_\" + attr] != undefined) return;\n"
    "\t\tif (e.attributes[attr] == undefined) return;\n"
    "\t\tif (val == undefined) val = e.attributes[attr].value;\n"
    "\t\te.setAttribute(\"_orig_\" + attr, val);\n"
    "\t}\n"
    "\tfunction orig_load(e, attr) {\n"
    "\t\tif (e.attributes[\"_orig_\"+attr] == undefined) return;\n"
    "\t\te.attributes[attr].value = e.attributes[\"_orig_\" + attr].value;\n"
    "\t\te.removeAttribute(\"_orig_\"+attr);\n"
    "\t}\n"
    "\tfunction g_to_text(e) {\n"
    "\t\tvar text = find_child(e, \"title\").firstChild.nodeValue;\n"
    "\t\treturn (text)\n"
    "\t}\n"
    "\tfunction g_to_func(e) {\n"
    "\t\tvar func = g_to_text(e);\n"
    "\t\t// if there's any manipulation we want to do to the function\n"
    "\t\t// name before it's searched, do it here before returning.\n"
    "\t\treturn (func);\n"
    "\t}\n"
    "\tfunction update_text(e) {\n"
    "\t\tvar r = find_child(e, \"rect\");\n"
    "\t\tvar t = find_child(e, \"text\");\n"
    "\t\tvar w = parseFloat(r.attributes.width.value) -3;\n"
    "\t\tvar txt = find_child(e, \"title\").textContent.replace(/\\([^(]*\\)$/,\"\");\n"
    "\t\tt.attributes.x.value = parseFloat(r.attributes.x.value) + 3;\n"
    "\n"
    "\t\t// Smaller than this size won't fit anything\n"
    "\t\tif (w < 2 * 12 * 0.59) {\n"
    "\t\t\tt.textContent = \"\";\n"
    "\t\t\treturn;\n"
    "\t\t}\n"
    "\n"
    "\t\tt.textContent = txt;\n"
    "\t\t// Fit in full text width\n"
    "\t\tif (/^ *$/.test(txt) || t.getSubStringLength(0, txt.length) < w)\n"
    "\t\t\treturn;\n"
    "\n"
    "\t\tfor (var x = txt.length - 2; x > 0; x--) {\n"
    "\t\t\tif (t.getSubStringLength(0, x + 2) <= w) {\n"
    "\t\t\t\tt.textContent = txt.substring(0, x) + \"..\";\n"
    "\t\t\t\treturn;\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\tt.textContent = \"\";\n"
    "\t}\n"
    "\n"
    "\t// zoom\n"
    "\tfunction zoom_reset(e) {\n"
    "\t\tif (e.attributes != undefined) {\n"
    "\t\t\torig_load(e, \"x\");\n"
    "\t\t\torig_load(e, \"width\");\n"
    "\t\t}\n"
    "\t\tif (e.childNodes == undefined) return;\n"
    "\t\tfor (var i = 0, c = e.childNodes; i < c.length; i++) {\n"
    "\t\t\tzoom_reset(c[i]);\n"
    "\t\t}\n"
    "\t}\n"
    "\tfunction zoom_child(e, x, ratio) {\n"
    "\t\tif (e.attributes != undefined) {\n"
    "\t\t\tif (e.attributes.x != undefined) {\n"
    "\t\t\t\torig_save(e, \"x\");\n"
    "\t\t\t\te.attributes.x.value = (parseFloat(e.attributes.x.value) - x - 10) * ratio + 10;\n"
    "\t\t\t\tif (e.tagName == \"text\")\n"
    "\t\t\t\t\te.attributes.x.value = find_child(e.parentNode, \"rect[x]\").attributes.x.value + 3;\n"
    "\t\t\t}\n"
    "\t\t\tif (e.attributes.width != undefined) {\n"
    "\t\t\t\torig_save(e, \"width\");\n"
    "\t\t\t\te.attributes.width.value = parseFloat(e.attributes.width.value) * ratio;\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\n"
    "\t\tif (e.childNodes == undefined) return;\n"
    "\t\tfor (var i = 0, c = e.childNodes; i < c.length; i++) {\n"
    "\t\t\tzoom_child(c[i], x - 10, ratio);\n"
    "\t\t}\n"
    "\t}\n"
    "\tfunction zoom_parent(e) {\n"
    "\t\tif (e.attributes) {\n"
    "\t\t\tif (e.attributes.x != undefined) {\n"
    "\t\t\t\torig_save(e, \"x\");\n"
    "\t\t\t\te.attributes.x.value = 10;\n"
    "\t\t\t}\n"
    "\t\t\tif (e.attributes.width != undefined) {\n"
    "\t\t\t\torig_save(e, \"width\");\n"
    "\t\t\t\te.attributes.width.value = parseInt(svg.width.baseVal.value) - (10 * 2);\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\tif (e.childNodes == undefined) return;\n"
    "\t\tfor (var i = 0, c = e.childNodes; i < c.length; i++) {\n"
    "\t\t\tzoom_parent(c[i]);\n"
    "\t\t}\n"
    "\t}\n"
    "\tfunction zoom(node) {\n"
    "\t\tvar attr = find_child(node, \"rect\").attributes;\n"
    "\t\tvar width = parseFloat(attr.width.value);\n"
    "\t\tvar xmin = parseFloat(attr.x.value);\n"
    "\t\tvar xmax = parseFloat(xmin + width);\n"
    "\t\tvar ymin = parseFloat(attr.y.value);\n"
    "\t\tvar ratio = (svg.width.baseVal.value - 2 * 10) / width;\n"
    "\n"
    "\t\t// XXX: Workaround for JavaScript float issues (fix me)\n"
    "\t\tvar fudge = 0.0001;\n"
    "\n"
    "\t\tunzoombtn.classList.remove(\"hide\");\n"
    "\n"
    "\t\tvar el = document.getElementById(\"frames\").children;\n"
    "\t\tfor (var i = 0; i < el.length; i++) {\n"
    "\t\t\tvar e = el[i];\n"
    "\t\t\tvar a = find_child(e, \"rect\").attributes;\n"
    "\t\t\tvar ex = parseFloat(a.x.value);\n"
    "\t\t\tvar ew = parseFloat(a.width.value);\n"
    "\t\t\tvar upstack;\n"
    "\t\t\t// Is it an ancestor\n"
    "\t\t\tif (%d == 0) {\n"
    "\t\t\t\tupstack = parseFloat(a.y.value) > ymin;\n"
    "\t\t\t} else {\n"
    "\t\t\t\tupstack = parseFloat(a.y.value) < ymin;\n"
    "\t\t\t}\n"
    "\t\t\tif (upstack) {\n"
    "\t\t\t\t// Direct ancestor\n"
    "\t\t\t\tif (ex <= xmin && (ex+ew+fudge) >= xmax) {\n"
    "\t\t\t\t\te.classList.add(\"parent\");\n"
    "\t\t\t\t\tzoom_parent(e);\n"
    "\t\t\t\t\tupdate_text(e);\n"
    "\t\t\t\t}\n"
    "\t\t\t\t// not in current path\n"
    "\t\t\t\telse\n"
    "\t\t\t\t\te.classList.add(\"hide\");\n"
    "\t\t\t}\n"
    "\t\t\t// Children maybe\n"
    "\t\t\telse {\n"
    "\t\t\t\t// no common path\n"
    "\t\t\t\tif (ex < xmin || ex + fudge >= xmax) {\n"
    "\t\t\t\t\te.classList.add(\"hide\");\n"
    "\t\t\t\t}\n"
    "\t\t\t\telse {\n"
    "\t\t\t\t\tzoom_child(e, xmin, ratio);\n"
    "\t\t\t\t\tupdate_text(e);\n"
    "\t\t\t\t}\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t}\n"
    "\tfunction unzoom() {\n"
    "\t\tunzoombtn.classList.add(\"hide\");\n"
    "\t\tvar el = document.getElementById(\"frames\").children;\n"
    "\t\tfor(var i = 0; i < el.length; i++) {\n"
    "\t\t\tel[i].classList.remove(\"parent\");\n"
    "\t\t\tel[i].classList.remove(\"hide\");\n"
    "\t\t\tzoom_reset(el[i]);\n"
    "\t\t\tupdate_text(el[i]);\n"
    "\t\t}\n"
    "\t}\n"
    "\n"
    "\t// search\n"
    "\tfunction reset_search() {\n"
    "\t\tvar el = document.querySelectorAll(\"#frames rect\");\n"
    "\t\tfor (var i = 0; i < el.length; i++) {\n"
    "\t\t\torig_load(el[i], \"fill\")\n"
    "\t\t}\n"
    "\t}\n"
    "\tfunction search_prompt() {\n"
    "\t\tif (!searching) {\n"
    "\t\t\tvar term = prompt(\"Enter a search term (regexp \" +\n"
    "\t\t\t    \"allowed, eg: ^ext4_)\", \"\");\n"
    "\t\t\tif (term != null) {\n"
    "\t\t\t\tsearch(term)\n"
    "\t\t\t}\n"
    "\t\t} else {\n"
    "\t\t\treset_search();\n"
    "\t\t\tsearching = 0;\n"
    "\t\t\tsearchbtn.classList.remove(\"show\");\n"
    "\t\t\tsearchbtn.firstChild.nodeValue = \"Search\"\n"
    "\t\t\tmatchedtxt.classList.add(\"hide\");\n"
    "\t\t\tmatchedtxt.firstChild.nodeValue = \"\"\n"
    "\t\t}\n"
    "\t}\n"
    "\tfunction search(term) {\n"
    "\t\tvar re = new RegExp(term);\n"
    "\t\tvar el = document.getElementById(\"frames\").children;\n"
    "\t\tvar matches = new Object();\n"
    "\t\tvar maxwidth = 0;\n"
    "\t\tfor (var i = 0; i < el.length; i++) {\n"
    "\t\t\tvar e = el[i];\n"
    "\t\t\tvar func = g_to_func(e);\n"
    "\t\t\tvar rect = find_child(e, \"rect\");\n"
    "\t\t\tif (func == null || rect == null)\n"
    "\t\t\t\tcontinue;\n"
    "\n"
    "\t\t\t// Save max width. Only works as we have a root frame\n"
    "\t\t\tvar w = parseFloat(rect.attributes.width.value);\n"
    "\t\t\tif (w > maxwidth)\n"
    "\t\t\t\tmaxwidth = w;\n"
    "\n"
    "\t\t\tif (func.match(re)) {\n"
    "\t\t\t\t// highlight\n"
    "\t\t\t\tvar x = parseFloat(rect.attributes.x.value);\n"
    "\t\t\t\torig_save(rect, \"fill\");\n"
    "\t\t\t\trect.attributes.fill.value = \"rgb(230,0,230)\";\n"
    "\n"
    "\t\t\t\t// remember matches\n"
    "\t\t\t\tif (matches[x] == undefined) {\n"
    "\t\t\t\t\tmatches[x] = w;\n"
    "\t\t\t\t} else {\n"
    "\t\t\t\t\tif (w > matches[x]) {\n"
    "\t\t\t\t\t\t// overwrite with parent\n"
    "\t\t\t\t\t\tmatches[x] = w;\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t}\n"
    "\t\t\t\tsearching = 1;\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\tif (!searching)\n"
    "\t\t\treturn;\n"
    "\n"
    "\t\tsearchbtn.classList.add(\"show\");\n"
    "\t\tsearchbtn.firstChild.nodeValue = \"Reset Search\";\n"
    "\n"
    "\t\t// calculate percent matched, excluding vertical overlap\n"
    "\t\tvar count = 0;\n"
    "\t\tvar lastx = -1;\n"
    "\t\tvar lastw = 0;\n"
    "\t\tvar keys = Array();\n"
    "\t\tfor (k in matches) {\n"
    "\t\t\tif (matches.hasOwnProperty(k))\n"
    "\t\t\t\tkeys.push(k);\n"
    "\t\t}\n"
    "\t\t// sort the matched frames by their x location\n"
    "\t\t// ascending, then width descending\n"
    "\t\tkeys.sort(function(a, b){\n"
    "\t\t\treturn a - b;\n"
    "\t\t});\n"
    "\t\t// Step through frames saving only the biggest bottom-up frames\n"
    "\t\t// thanks to the sort order. This relies on the tree property\n"
    "\t\t// where children are always smaller than their parents.\n"
    "\t\tvar fudge = 0.0001;\t// JavaScript floating point\n"
    "\t\tfor (var k in keys) {\n"
    "\t\t\tvar x = parseFloat(keys[k]);\n"
    "\t\t\tvar w = matches[keys[k]];\n"
    "\t\t\tif (x >= lastx + lastw - fudge) {\n"
    "\t\t\t\tcount += w;\n"
    "\t\t\t\tlastx = x;\n"
    "\t\t\t\tlastw = w;\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\t// display matched percent\n"
    "\t\tmatchedtxt.classList.remove(\"hide\");\n"
    "\t\tvar pct = 100 * count / maxwidth;\n"
    "\t\tif (pct != 100) pct = pct.toFixed(1)\n"
    "\t\tmatchedtxt.firstChild.nodeValue = \"Matched: \" + pct + \"%%\";\n"
    "\t}\n"
    "]]>\n"
    "</script>\n"
    "<rect x=\"0\" y=\"0\" width=\"100%%\" height=\"100%%\" fill=\"rgb(240,240,220)\"/>\n"
    "<text id=\"title\" x=\"%d\" y=\"%d\">%s</text>\n"
    "<text id=\"details\" x=\"%d\" y=\"%d\"> </text>\n"
    "<text id=\"unzoom\" x=\"%d\" y=\"%d\" class=\"hide\">Reset Zoom</text>\n"
    "<text id=\"search\" x=\"%d\" y=\"%d\">Search</text>\n"
    "<text id=\"matched\" x=\"%d\" y=\"%d\"> </text>\n"
    "<g id=\"frames\">\n";

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
    ".green {\n"
    "    color: #32c832;\n"
    "}\n"
    ".aqua {\n"
    "    color: #32a5a5;\n"
    "}\n"
    ".brown {\n"
    "    color: #be5a00;\n"
    "}\n"
    ".yellow {\n"
    "    color: #afaf32;\n"
    "}\n"
    ".red {\n"
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

    static std::string trim(const std::string& s, size_t maxchars) {
        if (maxchars < 3) {
            return "";
        } else if (s.length() > maxchars) {
            return s.substr(0, maxchars - 2) + "..";
        } else {
            return s;
        }
    }

    static void replace(std::string& s, char c, const char* replacement) {
        for (size_t i = 0; (i = s.find(c, i)) != std::string::npos; i++) {
            s.replace(i, 1, replacement);
        }
    }

    static void escape(std::string& s) {
        replace(s, '&', "&amp;");
        replace(s, '<', "&lt;");
        replace(s, '>', "&gt;");
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


class Palette {
  private:
    const char* _name;
    int _base;
    int _r, _g, _b;

  public:
    Palette(const char* name, int base, int r, int g, int b) : _name(name), _base(base), _r(r), _g(g), _b(b) {
    }

    const char* name() const {
        return _name;
    }

    int pickColor() const {
        double value = double(rand()) / RAND_MAX;
        return _base + (int(_r * value) << 16 | int(_g * value) << 8 | int(_b * value));
    }
};


void FlameGraph::dump(std::ostream& out, bool tree) {
    _scale = (_imagewidth - 20) / (double)_root._total;
    _pct = 100 / (double)_root._total;

    u64 cutoff = (u64)ceil(_minwidth / _scale);
    _imageheight = _frameheight * _root.depth(cutoff) + 70;

    if (tree) {
        printTreeHeader(out);
        printTreeFrame(out, _root, 0);
        printTreeFooter(out);
    } else {
        printHeader(out);
        printFrame(out, "all", _root, 10, _reverse ? 35 : (_imageheight - _frameheight - 35));
        printFooter(out);
    }
}

void FlameGraph::printHeader(std::ostream& out) {
    char buf[sizeof(SVG_HEADER) + 256];
    int x0 = _imagewidth / 2;
    int x1 = 10;
    int x2 = _imagewidth - 110;
    int y0 = 24;
    int y1 = _imageheight - 17;

    sprintf(buf, SVG_HEADER,
            _imagewidth, _imageheight, _imagewidth, _imageheight, _reverse,
            x0, y0, _title, x1, y1, x1, y0, x2, y0, x2, y1);
    out << buf;
}

void FlameGraph::printFooter(std::ostream& out) {
    out << "</g>\n</svg>\n";
}

double FlameGraph::printFrame(std::ostream& out, const std::string& name, const Trie& f, double x, double y) {
    double framewidth = f._total * _scale;

    // Skip too narrow frames, they are not important
    if (framewidth >= _minwidth) {
        std::string full_title = name;
        int color = selectFramePalette(full_title).pickColor();
        std::string short_title = StringUtils::trim(full_title, size_t(framewidth / 7));
        StringUtils::escape(full_title);
        StringUtils::escape(short_title);

        // Compensate rounding error in frame width
        double w = (round((x + framewidth) * 10) - round(x * 10)) / 10.0;

        snprintf(_buf, sizeof(_buf),
            "<g>\n"
            "<title>%s (%s samples, %.2f%%)</title><rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%d\" fill=\"#%06x\" rx=\"2\" ry=\"2\"/>\n"
            "<text x=\"%.1f\" y=\"%.1f\">%s</text>\n"
            "</g>\n",
            full_title.c_str(), Format().thousands(f._total), f._total * _pct, x, y, w, _frameheight - 1, color,
            x + 3, y + 3 + _frameheight * 0.5, short_title.c_str());
        out << _buf;

        x += f._self * _scale;
        y += _reverse ? _frameheight : -_frameheight;

        for (std::map<std::string, Trie>::const_iterator it = f._children.begin(); it != f._children.end(); ++it) {
            x += printFrame(out, it->first, it->second, x, y);
        }
    }

    return framewidth;
}

void FlameGraph::printTreeHeader(std::ostream& out) {
    char buf[sizeof(TREE_HEADER) + 256];
    const char* title = _reverse ? "Backtrace" : "Call tree";
    const char* counter = _counter ==  COUNTER_SAMPLES ? "samples" : "counter";
    sprintf(buf, TREE_HEADER, title, counter, Format().thousands(_root._total));
    out << buf;
}

void FlameGraph::printTreeFooter(std::ostream& out) {   
    out << TREE_FOOTER;
}

bool FlameGraph::printTreeFrame(std::ostream& out, const Trie& f, int depth) {
    double framewidth = f._total * _scale;
    if (framewidth < _minwidth) {
        return false;
    }

    std::vector<Node> subnodes;
    for (std::map<std::string, Trie>::const_iterator it = f._children.begin(); it != f._children.end(); ++it) {
        subnodes.push_back(Node(it->first, it->second));
    }
    std::sort(subnodes.begin(), subnodes.end());

    for (size_t i = 0; i < subnodes.size(); i++) {
        std::string full_title = subnodes[i]._name;
        const Trie* trie = subnodes[i]._trie;
        const char* color = selectFramePalette(full_title).name();
        StringUtils::escape(full_title);

        if (_reverse) {
            snprintf(_buf, sizeof(_buf),
                     "<li><div>[%d] %.2f%% %s</div><span class=\"%s\"> %s</span>\n",
                     depth,
                     trie->_total * _pct, Format().thousands(trie->_total),
                     color, full_title.c_str());
        } else {
            snprintf(_buf, sizeof(_buf),
                     "<li><div>[%d] %.2f%% %s self: %.2f%% %s</div><span class=\"%s\"> %s</span>\n",
                     depth,
                     trie->_total * _pct, Format().thousands(trie->_total),
                     trie->_self * _pct, Format().thousands(trie->_self),
                     color, full_title.c_str());
        }
        out << _buf;

        if (trie->_children.size() > 0) {
            out << "<ul>\n";
            if (!printTreeFrame(out, *trie, depth + 1)) {
                out << "<li>...\n";
            }
            out << "</ul>\n";
        }
    }

    return true;
}

const Palette& FlameGraph::selectFramePalette(std::string& name) {
    static const Palette
        green ("green",  0x50e150, 30, 30, 30),
        aqua  ("aqua",   0x50bebe, 30, 30, 30),
        brown ("brown",  0xe17d00, 30, 30,  0),
        yellow("yellow", 0xc8c83c, 30, 30, 10),
        red   ("red",    0xe15a5a, 30, 40, 40);

    if (StringUtils::endsWith(name, "_[j]", 4)) {
        // Java compiled frame
        name = name.substr(0, name.length() - 4);
        return green;
    } else if (StringUtils::endsWith(name, "_[i]", 4)) {
        // Java inlined frame
        name = name.substr(0, name.length() - 4);
        return aqua;
    } else if (StringUtils::endsWith(name, "_[k]", 4)) {
        // Kernel function
        name = name.substr(0, name.length() - 4);
        return brown;
    } else if (name.find("::") != std::string::npos || name.compare(0, 2, "-[") == 0 || name.compare(0, 2, "+[") == 0) {
        // C++ function or Objective C method
        return yellow;
    } else if ((int)name.find('/') > 0 || ((int)name.find('.') > 0 && name[0] >= 'A' && name[0] <= 'Z')) {
        // Java regular method
        return green;
    } else {
        // Other native code
        return red;
    }
}
