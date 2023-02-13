/*
 * Copyright 2022 Andrei Pangin
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

#ifndef _INCBIN_H
#define _INCBIN_H

#ifdef __APPLE__
#  define INCBIN_SECTION ".const_data"
#  define INCBIN_SYMBOL  "_"
#else
#  define INCBIN_SECTION ".section \".rodata\", \"a\", @progbits"
#  define INCBIN_SYMBOL
#endif

#define INCBIN(NAME, FILE) \
    extern const char NAME[];\
    extern const char NAME##_END[];\
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

#endif // _INCBIN_H
