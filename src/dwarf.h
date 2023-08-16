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


const int DW_REG_PLT = 128;      // denotes special rule for PLT entries
const int DW_REG_INVALID = 255;  // denotes unsupported configuration

const int DW_PC_OFFSET = 1;
const int DW_SAME_FP = 0x80000000;
const int DW_STACK_SLOT = sizeof(void*);


#if defined(__x86_64__)

#define DWARF_SUPPORTED true

const int DW_REG_FP = 6;
const int DW_REG_SP = 7;
const int DW_REG_PC = 16;
const int EMPTY_FRAME_SIZE = DW_STACK_SLOT;
const int LINKED_FRAME_SIZE = 2 * DW_STACK_SLOT;

#elif defined(__i386__)

#define DWARF_SUPPORTED true

const int DW_REG_FP = 5;
const int DW_REG_SP = 4;
const int DW_REG_PC = 8;
const int EMPTY_FRAME_SIZE = DW_STACK_SLOT;
const int LINKED_FRAME_SIZE = 2 * DW_STACK_SLOT;

#elif defined(__aarch64__)

#define DWARF_SUPPORTED true

const int DW_REG_FP = 29;
const int DW_REG_SP = 31;
const int DW_REG_PC = 30;
const int EMPTY_FRAME_SIZE = 0;
const int LINKED_FRAME_SIZE = 0;

#else

#define DWARF_SUPPORTED false

const int DW_REG_FP = 0;
const int DW_REG_SP = 1;
const int DW_REG_PC = 2;
const int EMPTY_FRAME_SIZE = 0;
const int LINKED_FRAME_SIZE = 0;

#endif


struct FrameDesc {
    u32 loc;
    int cfa;
    int fp_off;

    static FrameDesc default_frame;

    static int comparator(const void* p1, const void* p2) {
        FrameDesc* fd1 = (FrameDesc*)p1;
        FrameDesc* fd2 = (FrameDesc*)p2;
        return (int)(fd1->loc - fd2->loc);
    }
};


class DwarfParser {
  private:
    const char* _name;
    const char* _image_base;
    const char* _ptr;

    int _capacity;
    int _count;
    FrameDesc* _table;
    FrameDesc* _prev;

    u32 _code_align;
    int _data_align;

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

    void parse(const char* eh_frame_hdr);
    void parseCie();
    void parseFde();
    void parseInstructions(u32 loc, const char* end);
    int parseExpression();

    void addRecord(u32 loc, u32 cfa_reg, int cfa_off, int fp_off);
    FrameDesc* addRecordRaw(u32 loc, int cfa, int fp_off);

  public:
    DwarfParser(const char* name, const char* image_base, const char* eh_frame_hdr);

    FrameDesc* table() const {
        return _table;
    }

    int count() const {
        return _count;
    }
};

#endif // _DWARF_H
