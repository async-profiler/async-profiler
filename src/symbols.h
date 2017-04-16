/*
 * Copyright 2017 Andrei Pangin
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

#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include <elf.h>
#include "codeCache.h"


typedef Elf64_Ehdr ElfHeader;
typedef Elf64_Shdr ElfSection;
typedef Elf64_Nhdr ElfNote;
typedef Elf64_Sym  ElfSymbol;

class ElfParser {
  private:
    CodeCache* _cc;
    const char* _base;
    const char* _file_name;
    ElfHeader* _header;
    const char* _sections;

    ElfParser(CodeCache* cc, const char* base, const void* addr, const char* file_name = NULL) {
        _cc = cc;
        _base = base;
        _file_name = file_name;
        _header = (ElfHeader*)addr;
        _sections = (const char*)addr + _header->e_shoff;
    }

    bool valid_header() {
        unsigned char* ident = _header->e_ident;
        return ident[0] == 0x7f && ident[1] == 'E' && ident[2] == 'L' && ident[3] == 'F'
            && ident[4] == ELFCLASS64 && ident[5] == ELFDATA2LSB && ident[6] == EV_CURRENT
            && _header->e_shstrndx != SHN_UNDEF;
    }

    ElfSection* section(int index) {
        return (ElfSection*)(_sections + index * _header->e_shentsize);
    }

    const char* at(ElfSection* section) {
        return (const char*)_header + section->sh_offset;
    }

    ElfSection* findSection(uint32_t type, const char* name);

    void loadSymbols(bool use_debug);
    bool loadSymbolsUsingBuildId();
    bool loadSymbolsUsingDebugLink();
    void loadSymbolTable(ElfSection* symtab);

  public:
    static bool parseFile(CodeCache* cc, const char* base, const char* file_name, bool use_debug);
    static void parseMem(CodeCache* cc, const char* base, const void* addr);
};


class Symbols {
  private:
    static void parseKernelSymbols(CodeCache* cc);

  public:
    static int parseMaps(CodeCache** array, int size);
};

#endif // _SYMBOLS_H
