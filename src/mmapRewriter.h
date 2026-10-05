/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
#ifndef _MMAPREWRITER_H
#define _MMAPREWRITER_H
#include <string.h>
#include <vector>
#include <string>
#include "classfile_constants.h"
#include "arch.h"

// GETFIELD peer:J and INVOKESTATIC address(Object):J both consume one reference
// and produce one long; both occupy three bytes. Branch offsets, stack maps and
// exception tables therefore remain unchanged. Only JNA's dispatch method changes.
class MmapClassReader {
  public:
    const u8* data;
    size_t size, pos;
    bool ok;
    MmapClassReader(const u8* d, size_t n) : data(d), size(n), pos(0), ok(true) {}
    unsigned read(int n) {
        if (!ok || pos + n > size) { ok = false; return 0; }
        unsigned v = 0;
        while (n--) v = (v << 8) | data[pos++];
        return v;
    }
    void skip(size_t n) { if (n > size - pos) ok = false; else pos += n; }
};
static bool rewriteMmapClass(const u8* data, size_t len, std::vector<u8>& output) {
    MmapClassReader r(data, len);
    if (r.read(4) != 0xcafebabe) return false;
    r.skip(4);
    unsigned count = r.read(2);
    if (count == 0 || count > 65529) return false;
    struct CP { unsigned tag = 0, a = 0, b = 0; std::string text; };
    std::vector<CP> pool(count);
    for (unsigned i = 1; i < count && r.ok; i++) {
        CP& c = pool[i];
        c.tag = r.read(1);
        switch (c.tag) {
            case 1: {
                unsigned n = r.read(2); size_t p = r.pos; r.skip(n);
                if (r.ok) c.text.assign((const char*)data + p, n);
                break;
            }
            case 7: case 8: case 16: case 19: case 20: c.a = r.read(2); break;
            case 9: case 10: case 11: case 12: case 17: case 18: c.a = r.read(2); c.b = r.read(2); break;
            case 3: case 4: r.skip(4); break;
            case 5: case 6: r.skip(8); if (++i >= count) return false; break;
            case 15: r.skip(3); break;
            default: return false;
        }
    }
    if (!r.ok) return false;
    size_t cp_end = r.pos;
    std::vector<size_t> patches;
    r.skip(6);
    unsigned interfaces = r.read(2); r.skip(interfaces * 2);
    for (int section = 0; section < 2 && r.ok; section++) {
        unsigned members = r.read(2);
        for (unsigned m = 0; m < members && r.ok; m++) {
            r.skip(2); unsigned name = r.read(2), desc = r.read(2), attributes = r.read(2);
            if (name >= count || desc >= count) return false;
            bool target = section == 1 && pool[name].text == "invoke" &&
                pool[desc].text == "([Ljava/lang/Object;Ljava/lang/Class;ZI)Ljava/lang/Object;";
            for (unsigned a = 0; a < attributes && r.ok; a++) {
                unsigned attr = r.read(2), length = r.read(4);
                size_t start = r.pos;
                if (attr >= count || length > len - start) return false;
                if (target && pool[attr].text == "Code") {
                    r.skip(4); unsigned bytes = r.read(4); size_t code = r.pos;
                    if (!r.ok || bytes > len - code || bytes + 8 > length) return false;
                    static const unsigned char sizes[JVM_OPC_MAX + 1] = JVM_OPCODE_LENGTH_INITIALIZER;
                    for (size_t pc = 0; pc < bytes;) {
                        unsigned op = data[code + pc];
                        size_t step = op <= JVM_OPC_MAX ? sizes[op] : 0;
                        if (op == JVM_OPC_wide) {
                            if (pc + 1 >= bytes) return false;
                            step = data[code + pc + 1] == JVM_OPC_iinc ? 6 : 4;
                        } else if (op == JVM_OPC_tableswitch || op == JVM_OPC_lookupswitch) {
                            size_t aligned = (pc + 4) & ~(size_t)3;
                            MmapClassReader sw(data + code, bytes); sw.pos = aligned;
                            sw.read(4);
                            if (op == JVM_OPC_tableswitch) {
                                int low = sw.read(4), high = sw.read(4);
                                if (!sw.ok || high < low) return false;
                                unsigned long long n = (long long)high - low + 1;
                                if (n > bytes / 4) return false;
                                step = aligned - pc + 12 + n * 4;
                            } else {
                                unsigned n = sw.read(4);
                                if (!sw.ok || n > bytes / 8) return false;
                                step = aligned - pc + 8 + n * 8;
                            }
                        }
                        if (!step || step > bytes - pc) return false;
                        if (op == JVM_OPC_getfield) {
                            unsigned ref = data[code + pc + 1] * 256 + data[code + pc + 2];
                            if (ref >= count || pool[ref].tag != 9) return false;
                            unsigned nt = pool[ref].b, cls = pool[ref].a;
                            if (nt >= count || cls >= count || pool[nt].a >= count || pool[nt].b >= count || pool[cls].a >= count) return false;
                            std::string owner = pool[pool[cls].a].text;
                            if (pool[pool[nt].a].text == "peer" && pool[pool[nt].b].text == "J" &&
                                (owner == "com/sun/jna/Function" || owner == "com/sun/jna/Pointer")) patches.push_back(code + pc);
                        }
                        pc += step;
                    }
                }
                r.pos = start + length;
            }
        }
    }
    if (!r.ok || patches.empty()) return false;
    std::vector<u8> extra;
    auto put = [&](unsigned v) { extra.push_back(v >> 8); extra.push_back(v); };
    auto utf = [&](const char* s) { extra.push_back(1); put(strlen(s)); extra.insert(extra.end(), s, s + strlen(s)); };
    utf("one/profiler/MmapBridge");       // count
    extra.push_back(7); put(count);       // count + 1: Class
    utf("address");                     // count + 2
    utf("(Ljava/lang/Object;)J");        // count + 3
    extra.push_back(12); put(count + 2); put(count + 3); // count + 4
    extra.push_back(10); put(count + 1); put(count + 4); // count + 5
    output.assign(data, data + cp_end);
    output[8] = (count + 6) >> 8; output[9] = count + 6;
    output.insert(output.end(), extra.begin(), extra.end());
    output.insert(output.end(), data + cp_end, data + len);
    for (size_t p : patches) {
        p += extra.size();
        output[p] = JVM_OPC_invokestatic;
        output[p + 1] = (count + 5) >> 8; output[p + 2] = count + 5;
    }
    return true;
}
#endif
