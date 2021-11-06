/*
 * Copyright 2021 Andrei Pangin
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

#ifndef _DWARF_H
#define _DWARF_H

#include <stddef.h>
#include "arch.h"


enum {
    DW_PC_OFFSET = 1,
    DW_SAME_FP = 2,
    DW_CFA_PLT = 128
};


struct FrameDesc {
    u32 loc;
    int cfa;
    int fp_off;

    static int comparator(const void* p1, const void* p2) {
        FrameDesc* fd1 = (FrameDesc*)p1;
        FrameDesc* fd2 = (FrameDesc*)p2;
        return (int)(fd1->loc - fd2->loc);
    }
};


class DwarfParser {
  private:
    const char* _eh_frame;
    const char* _ptr;
    const char* _image_base;

    int _capacity;
    int _count;
    FrameDesc* _table;
    FrameDesc* _prev;

    const char* add(size_t size) {
        const char* ptr = _ptr;
        _ptr = ptr + size;
        return ptr;
    }

    u8 get8() {
        return *_ptr++;
    }

    u16 get16() {
        return *(u16*)add(2);
    }

    u32 get32() {
        return *(u32*)add(4);
    }

    u32 getLeb() {
        u32 result = 0;
        for (u32 shift = 0; ; shift += 7) {
            u8 b = *_ptr++;
            result |= (b & 0x7f) << shift;
            if ((b & 0x80) == 0) {
                return result;
            }
        }
    }

    int getSLeb() {
        int result = 0;
        for (u32 shift = 0; ; shift += 7) {
            u8 b = *_ptr++;
            result |= (b & 0x7f) << shift;
            if ((b & 0x80) == 0) {
                if ((b & 0x40) != 0 && (shift += 7) < 32) {
                    result |= -1 << shift;
                }
                return result;
            }
        }
    }

    void skipLeb() {
        while (*_ptr++ & 0x80) {}
    }

    const char* getPtr() {
        const char* ptr = _ptr;
        return ptr + *(int*)add(4);
    }

    void parse();
    void parseInstructions(u32 loc, const char* end);
    int parseExpression();

    void addRecord(u32 loc, u32 cfa_reg, int cfa_off, int fp_off);
    void addRecordRaw(u32 loc, int cfa, int fp_off);

  public:
    DwarfParser(const char* eh_frame, const char* image_base);

    FrameDesc* table() const {
        return _table;
    }

    int count() const {
        return _count;
    }
};

#endif // _DWARF_H
