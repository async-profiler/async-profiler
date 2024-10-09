/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCBIN_H
#define _INCBIN_H

#ifdef __APPLE__
#  define INCBIN_SECTION ".const_data"
#  define INCBIN_SYMBOL  "_"
#else
#  define INCBIN_SECTION ".section \".rodata\", \"a\""
#  define INCBIN_SYMBOL
#endif

#define INCBIN(NAME, FILE) \
    extern "C" const char NAME[];\
    extern "C" const char NAME##_END[];\
    asm(INCBIN_SECTION "\n"\
        ".global " INCBIN_SYMBOL #NAME "\n"\
        INCBIN_SYMBOL #NAME ":\n"\
        ".incbin \"" FILE "\"\n"\
        ".global " INCBIN_SYMBOL #NAME "_END\n"\
        INCBIN_SYMBOL #NAME "_END:\n"\
        ".byte 0x00\n"\
        ".previous\n"\
    );

#define INCBIN_SIZEOF(NAME) (NAME##_END - NAME)

#define INCLUDE_HELPER_CLASS(NAME_VAR, DATA_VAR, NAME) \
    static const char* const NAME_VAR = NAME;\
    INCBIN(DATA_VAR, "src/helper/" NAME ".class")

#endif // _INCBIN_H
