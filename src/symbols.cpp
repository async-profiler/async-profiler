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

#include <elf.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include "symbols.h"


class SymbolMap {
  private:
    const char* _addr;
    const char* _type;

  public:
      SymbolMap(const char* s) {
          _addr = s;
          _type = strchr(_addr, ' ') + 1;
      }

      const char* addr() { return (const char*)strtoul(_addr, NULL, 16); }
      char type()        { return _type[0]; }
      const char* name() { return _type + 2; }
};

class MemoryMap {
  private:
    const char* _addr;
    const char* _end;
    const char* _perm;
    const char* _offs;
    const char* _device;
    const char* _inode;
    const char* _file;

  public:
      MemoryMap(const char* s) {
          _addr = s;
          _end = strchr(_addr, '-') + 1;
          _perm = strchr(_end, ' ') + 1;
          _offs = strchr(_perm, ' ') + 1;
          _device = strchr(_offs, ' ') + 1;
          _inode = strchr(_device, ' ') + 1;
          _file = strchr(_inode, ' ') + 1;

          while (*_file == ' ') _file++;
      }

      const char* file()    { return _file; }
      bool isExecutable()   { return _perm[0] == 'r' && _perm[2] == 'x'; }
      const char* addr()    { return (const char*)strtoul(_addr, NULL, 16); }
      const char* end()     { return (const char*)strtoul(_end, NULL, 16); }
      unsigned long offs()  { return strtoul(_offs, NULL, 16); }
      unsigned long inode() { return strtoul(_inode, NULL, 10); }
};


void Symbols::parseKernelSymbols(CodeCache* cc) {
    std::ifstream maps("/proc/kallsyms");
    std::string str;

    while (std::getline(maps, str)) {
        str += "_[k]";
        SymbolMap map(str.c_str());
        char type = map.type();
        if (type == 'T' || type == 't' || type == 'V' || type == 'v' || type == 'W' || type == 'w') {
            const char* addr = map.addr();
            if (addr != NULL) {
                cc->add(addr, 0, cc->addString(map.name()));
            }
        }
    }

    cc->sort();
}

void Symbols::parseElf(CodeCache* cc, const char* addr, const char* base) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)addr;
    unsigned char* ident = ehdr->e_ident;
    if (ident[0] != 0x7f || ident[1] != 'E' || ident[2] != 'L' || ident[3] != 'F' ||
        ident[4] != ELFCLASS64 || ident[5] != ELFDATA2LSB || ident[6] != EV_CURRENT) {
        return;
    }

    const char* sections = addr + ehdr->e_shoff;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr* section = (Elf64_Shdr*)(sections + i * ehdr->e_shentsize);
        if (section->sh_type == SHT_SYMTAB || section->sh_type == SHT_DYNSYM) {
            Elf64_Shdr* strtab = (Elf64_Shdr*)(sections + section->sh_link * ehdr->e_shentsize);
            const char* strings = cc->addStrings(addr + strtab->sh_offset, strtab->sh_size);

            const char* symtab = addr + section->sh_offset;
            const char* symtab_end = symtab + section->sh_size;
            for (; symtab < symtab_end; symtab += section->sh_entsize) {
                Elf64_Sym* sym = (Elf64_Sym*)symtab;
                if (sym->st_name != 0 && sym->st_value != 0) {
                    cc->add(base + sym->st_value, (int)sym->st_size, strings + sym->st_name);
                }
            }
        }
    }

    cc->sort();
}

void Symbols::parseFile(CodeCache* cc, const char* fileName, const char* base) {
    int fd = open(fileName, O_RDONLY);
    if (fd != -1) {
        size_t length = (size_t)lseek64(fd, 0, SEEK_END);
        char* addr = (char*)mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
        if (addr != NULL) {
            parseElf(cc, addr, base);
            munmap(addr, length);
        }
        close(fd);
    }
}

int Symbols::parseMaps(CodeCache** array, int size) {
    int count = 0;
    if (count < size) {
        CodeCache* cc = new CodeCache("[kernel]", true);
        parseKernelSymbols(cc);
        array[count++] = cc;
    }

    std::ifstream maps("/proc/self/maps");
    std::string str;

    while (count < size && std::getline(maps, str)) {
        MemoryMap map(str.c_str());
        if (map.isExecutable() && map.file()[0] != 0) {
            CodeCache* cc = new CodeCache(map.file(), false, map.addr(), map.end());
            const char* base = map.addr() - map.offs();
            if (map.inode() != 0) {
                parseFile(cc, map.file(), base);
            } else if (strcmp(map.file(), "[vdso]") == 0) {
                parseElf(cc, base, base);
            }
            array[count++] = cc;
        }
    }

    return count;
}
