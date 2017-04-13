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
#include <linux/limits.h>
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


void BuildId::set(const char* value, int length) {
    if (length > 0 && length <= sizeof(_value)) {
        memcpy(_value, value, length);
        _length = length;
    }
}


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
}

void Symbols::parseLibrarySymbols(CodeCache* cc, const char* lib_name, const char* base) {
    // Parse symbols from the original .so
    // If there is no .strtab section, try loading symbols from an external debug library
    BuildId build_id;
    if (parseFile(cc, lib_name, base, &build_id) && cc->hasDebugSymbols()) {
        return;
    }

    char path[PATH_MAX];

    // 1. /usr/lib/debug/.build-id/ab/cdef1234.debug
    if (build_id.length() > 1) {
        char* p = path + sprintf(path, "/usr/lib/debug/.build-id/%02hhx/", build_id[0]);
        for (int i = 1; i < build_id.length(); i++) {
            p += sprintf(p, "%02hhx", build_id[i]);
        }
        strcpy(p, ".debug");
        if (parseFile(cc, path, base, NULL)) return;
    }

    // 2. /usr/lib/debug/path/to/lib.so
    if (snprintf(path, sizeof(path), "/usr/lib/debug%s", lib_name) < sizeof(path)) {
        if (parseFile(cc, path, base, NULL)) return;
    }

    // 3. /usr/lib/debug/path/to/lib.so.debug
    if (snprintf(path, sizeof(path), "/usr/lib/debug%s.debug", lib_name) < sizeof(path)) {
        if (parseFile(cc, path, base, NULL)) return;
    }

    // 4. /path/to/lib.so.debug
    if (snprintf(path, sizeof(path), "%s.debug", lib_name) < sizeof(path)) {
        if (parseFile(cc, path, base, NULL)) return;
    }
}

bool Symbols::parseFile(CodeCache* cc, const char* file_name, const char* base, BuildId* build_id) {
    int fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    size_t length = (size_t)lseek64(fd, 0, SEEK_END);
    char* addr = (char*)mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr != NULL) {
        parseElf(cc, addr, base, build_id);
        munmap(addr, length);
    }
    close(fd);
    return true;
}

void Symbols::parseElf(CodeCache* cc, const char* addr, const char* base, BuildId* build_id) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)addr;
    unsigned char* ident = ehdr->e_ident;
    if (ident[0] != 0x7f || ident[1] != 'E' || ident[2] != 'L' || ident[3] != 'F' ||
        ident[4] != ELFCLASS64 || ident[5] != ELFDATA2LSB || ident[6] != EV_CURRENT) {
        return;
    }

    const char* sections = addr + ehdr->e_shoff;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr* section = (Elf64_Shdr*)(sections + i * ehdr->e_shentsize);
        if (section->sh_type == SHT_SYMTAB || section->sh_type == SHT_DYNSYM && build_id != NULL) {
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

            if (section->sh_type == SHT_SYMTAB) {
                cc->setDebugSymbols(true);
            }
        } else if (section->sh_type == SHT_NOTE && section->sh_size > 16 && build_id != NULL) {
            Elf64_Nhdr* note = (Elf64_Nhdr*)(addr + section->sh_offset);
            const char* data = (const char*)(note + 1);
            if (note->n_namesz == 4 && note->n_type == NT_GNU_BUILD_ID && memcmp(data, "GNU", 4) == 0) {
                build_id->set(data + 4, note->n_descsz);
            }
        }
    }
}

int Symbols::parseMaps(CodeCache** array, int size) {
    int count = 0;
    if (count < size) {
        CodeCache* cc = new CodeCache("[kernel]");
        parseKernelSymbols(cc);
        cc->sort();
        array[count++] = cc;
    }

    std::ifstream maps("/proc/self/maps");
    std::string str;

    while (count < size && std::getline(maps, str)) {
        MemoryMap map(str.c_str());
        if (map.isExecutable() && map.file()[0] != 0) {
            CodeCache* cc = new CodeCache(map.file(), map.addr(), map.end());
            const char* base = map.addr() - map.offs();

            if (map.inode() != 0) {
                parseLibrarySymbols(cc, map.file(), base);
            } else if (strcmp(map.file(), "[vdso]") == 0) {
                parseElf(cc, base, base, NULL);
            }

            cc->sort();
            array[count++] = cc;
        }
    }

    return count;
}
