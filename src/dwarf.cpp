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

#include <stdlib.h>
#include <map>
#include "dwarf.h"
#include "log.h"


enum {
    DW_CFA_nop                = 0x0,
    DW_CFA_set_loc            = 0x1,
    DW_CFA_advance_loc1       = 0x2,
    DW_CFA_advance_loc2       = 0x3,
    DW_CFA_advance_loc4       = 0x4,
    DW_CFA_offset_extended    = 0x5,
    DW_CFA_restore_extended   = 0x6,
    DW_CFA_undefined          = 0x7,
    DW_CFA_same_value         = 0x8,
    DW_CFA_register           = 0x9,
    DW_CFA_remember_state     = 0xa,
    DW_CFA_restore_state      = 0xb,
    DW_CFA_def_cfa            = 0xc,
    DW_CFA_def_cfa_register   = 0xd,
    DW_CFA_def_cfa_offset     = 0xe,
    DW_CFA_def_cfa_expression = 0xf,
    DW_CFA_expression         = 0x10,
    DW_CFA_offset_extended_sf = 0x11,
    DW_CFA_def_cfa_sf         = 0x12,
    DW_CFA_def_cfa_offset_sf  = 0x13,
    DW_CFA_val_offset         = 0x14,
    DW_CFA_val_offset_sf      = 0x15,
    DW_CFA_val_expression     = 0x16,
    DW_CFA_GNU_args_size      = 0x2e,

    DW_CFA_advance_loc        = 0x1,
    DW_CFA_offset             = 0x2,
    DW_CFA_restore            = 0x3,
};

enum {
    DW_OP_breg_pc = 0x80,
    DW_OP_const1u = 0x08,
    DW_OP_const1s = 0x09,
    DW_OP_const2u = 0x0a,
    DW_OP_const2s = 0x0b,
    DW_OP_const4u = 0x0c,
    DW_OP_const4s = 0x0d,
    DW_OP_constu  = 0x10,
    DW_OP_consts  = 0x11,
    DW_OP_minus   = 0x1c,
    DW_OP_plus    = 0x22,
};


DwarfParser::DwarfParser(const char* eh_frame, const char* image_base) :
    _eh_frame(eh_frame),
    _ptr(eh_frame),
    _image_base(image_base)
{
    _capacity = 128;
    _count = 0;
    _table = (FrameDesc*)malloc(_capacity * sizeof(FrameDesc));

    parse();
}

void DwarfParser::parse() {
    std::map<u32, bool> gaps;

    u32 fde_len;
    while ((fde_len = get32()) != 0) {
        const char* end = _ptr + fde_len;

        int cie_offset = get32();
        if (cie_offset != 0) {
            // FDE
            u32 range_start = getPtr() - _image_base;
            u32 range_len = get32();
            _ptr += getLeb();
            parseInstructions(range_start, end);

            gaps[range_start] = true;
            gaps[range_start + range_len];
        } else if (_count == 0) {
            // The first CIE
            _ptr++;
            while (*_ptr++) {}
            skipLeb();
            skipLeb();
            skipLeb();
            _ptr += getLeb();
            parseInstructions(0, end);
        }

        _ptr = end;
    }

    // TODO: find a better solution?
    for (std::map<u32, bool>::const_iterator it = gaps.begin(); it != gaps.end(); ++it) {
        if (!it->second) {
            addRecordRaw(it->first, _table[0].cfa, _table[0].fp_off);
        }
    }

    // TODO: expensive; postpone until later
    qsort(_table, _count, sizeof(FrameDesc), FrameDesc::comparator);
}

void DwarfParser::parseInstructions(u32 loc, const char* end) {
    const u32 fp = 6;
    const u32 sp = 7;
    const u32 pc = 16;
    const int data_align = -8;

    u32 cfa_reg = sp;
    int cfa_off = -data_align;
    int fp_off = DW_SAME_FP;

    u32 rem_cfa_reg = 0;
    int rem_cfa_off = 0;
    int rem_fp_off = 0;

    _prev = NULL;

    while (_ptr < end) {
        u8 op = get8();
        switch (op >> 6) {
            case 0:
                switch (op) {
                    case DW_CFA_nop:
                    case DW_CFA_set_loc:
                        _ptr = end;
                        break;
                    case DW_CFA_advance_loc1:
                        addRecord(loc, cfa_reg, cfa_off, fp_off);
                        loc += get8();
                        break;
                    case DW_CFA_advance_loc2:
                        addRecord(loc, cfa_reg, cfa_off, fp_off);
                        loc += get16();
                        break;
                    case DW_CFA_advance_loc4:
                        addRecord(loc, cfa_reg, cfa_off, fp_off);
                        loc += get32();
                        break;
                    case DW_CFA_offset_extended:
                        if (getLeb() == fp) {
                            fp_off = getLeb() * data_align;
                        } else {
                            skipLeb();
                        }
                        break;
                    case DW_CFA_restore_extended:
                    case DW_CFA_undefined:
                    case DW_CFA_same_value:
                        skipLeb();
                        break;
                    case DW_CFA_register:
                        skipLeb();
                        skipLeb();
                        break;
                    case DW_CFA_remember_state:
                        rem_cfa_reg = cfa_reg;
                        rem_cfa_off = cfa_off;
                        rem_fp_off = fp_off;
                        break;
                    case DW_CFA_restore_state:
                        cfa_reg = rem_cfa_reg;
                        cfa_off = rem_cfa_off;
                        fp_off = rem_fp_off;
                        break;
                    case DW_CFA_def_cfa:
                        cfa_reg = getLeb();
                        cfa_off = getLeb();
                        break;
                    case DW_CFA_def_cfa_register:
                        cfa_reg = getLeb();
                        break;
                    case DW_CFA_def_cfa_offset:
                        cfa_off = getLeb();
                        break;
                    case DW_CFA_def_cfa_expression: {
                        u32 len = getLeb();
                        cfa_reg = len == 11 ? DW_CFA_PLT : 0;
                        cfa_off = sizeof(void*);
                        _ptr += len;
                        break;
                    }
                    case DW_CFA_expression:
                        skipLeb();
                        _ptr += getLeb();
                        break;
                    case DW_CFA_offset_extended_sf:
                        if (getLeb() == fp) {
                            fp_off = getSLeb() * data_align;
                        } else {
                            skipLeb();
                        }
                        break;
                    case DW_CFA_def_cfa_sf:
                        cfa_reg = getLeb();
                        cfa_off = getSLeb() * data_align;
                        break;
                    case DW_CFA_def_cfa_offset_sf:
                        cfa_off = getSLeb() * data_align;
                        break;
                    case DW_CFA_val_offset:
                    case DW_CFA_val_offset_sf:
                        skipLeb();
                        skipLeb();
                        break;
                    case DW_CFA_val_expression:
                        if (getLeb() == pc) {
                            int pc_off = parseExpression();
                            if (pc_off != 0) {
                                fp_off = DW_PC_OFFSET | (pc_off << 1);
                            }
                        } else {
                            _ptr += getLeb();
                        }
                        break;
                    case DW_CFA_GNU_args_size:
                        skipLeb();
                        break;
                    default:
                        Log::warn("Unknown DWARF instruction 0x%x\n", op);
                        return;
                }
                break;
            case DW_CFA_advance_loc:
                addRecord(loc, cfa_reg, cfa_off, fp_off);
                loc += op & 0x3f;
                break;
            case DW_CFA_offset:
                if ((op & 0x3f) == fp) {
                    fp_off = getLeb() * data_align;
                } else {
                    skipLeb();
                }
                break;
            case DW_CFA_restore:
                break;
        }
    }

    addRecord(loc, cfa_reg, cfa_off, fp_off);
}

int DwarfParser::parseExpression() {
    int pc_off = 0;
    int tos = 0;

    u32 len = getLeb();
    const char* end = _ptr + len;

    while (_ptr < end) {
        u8 op = get8();
        switch (op) {
            case DW_OP_breg_pc:
                pc_off = getSLeb();
                break;
            case DW_OP_const1u:
                tos = get8();
                break;
            case DW_OP_const1s:
                tos = (signed char)get8();
                break;
            case DW_OP_const2u:
                tos = get16();
                break;
            case DW_OP_const2s:
                tos = (short)get16();
                break;
            case DW_OP_const4u:
            case DW_OP_const4s:
                tos = get32();
                break;
            case DW_OP_constu:
                tos = getLeb();
                break;
            case DW_OP_consts:
                tos = getSLeb();
                break;
            case DW_OP_minus:
                pc_off -= tos;
                break;
            case DW_OP_plus:
                pc_off += tos;
                break;
            default:
                Log::warn("Unknown DWARF opcode 0x%x\n", op);
                return 0;
        }
    }

    return pc_off;
}

void DwarfParser::addRecord(u32 loc, u32 cfa_reg, int cfa_off, int fp_off) {
    int cfa = cfa_reg | cfa_off << 8;
    if (_prev == NULL || _prev->cfa != cfa || _prev->fp_off != fp_off) {
        addRecordRaw(loc, cfa, fp_off);
    }
}

void DwarfParser::addRecordRaw(u32 loc, int cfa, int fp_off) {
    if (_count >= _capacity) {
        _capacity *= 2;
        _table = (FrameDesc*)realloc(_table, _capacity * sizeof(FrameDesc));
    }

    FrameDesc* f = &_table[_count++];
    f->loc = loc;
    f->cfa = cfa;
    f->fp_off = fp_off;
    _prev = f;
}
