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


class SymbolDesc {
  private:
    const char* _addr;
    const char* _type;

  public:
      SymbolDesc(const char* s) {
          _addr = s;
          _type = strchr(_addr, ' ') + 1;
      }

      const char* addr() { return (const char*)strtoul(_addr, NULL, 16); }
      char type()        { return _type[0]; }
      const char* name() { return _type + 2; }
};

class MemoryMapDesc {
  private:
    const char* _addr;
    const char* _end;
    const char* _perm;
    const char* _offs;
    const char* _device;
    const char* _inode;
    const char* _file;

  public:
      MemoryMapDesc(const char* s) {
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


ElfSection* ElfParser::findSection(uint32_t type, const char* name) {
    const char* strtab = at(section(_header->e_shstrndx));

    for (int i = 0; i < _header->e_shnum; i++) {
        ElfSection* section = this->section(i);
        if (section->sh_type == type && section->sh_name != 0) {
            if (strcmp(strtab + section->sh_name, name) == 0) {
                return section;
            }
        }
    }

    return NULL;
}

bool ElfParser::parseFile(NativeCodeCache* cc, const char* base, const char* file_name, bool use_debug) {
    int fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    size_t length = (size_t)lseek64(fd, 0, SEEK_END);
    void* addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (addr != NULL) {
        ElfParser elf(cc, base, addr, file_name);
        elf.loadSymbols(use_debug);
        munmap(addr, length);
    }
    return true;
}

void ElfParser::parseMem(NativeCodeCache* cc, const char* base, const void* addr) {
    ElfParser elf(cc, base, addr);
    elf.loadSymbols(false);
}

void ElfParser::loadSymbols(bool use_debug) {
    if (!valid_header()) {
        return;
    }

    // Look for debug symbols in the original .so
    ElfSection* section = findSection(SHT_SYMTAB, ".symtab");
    if (section != NULL) {
        loadSymbolTable(section);
        return;
    }

    // Try to load symbols from an external debuginfo library
    if (use_debug) {
        if (loadSymbolsUsingBuildId() || loadSymbolsUsingDebugLink()) {
            return;
        }
    }

    // If everything else fails, load only exported symbols
    section = findSection(SHT_DYNSYM, ".dynsym");
    if (section != NULL) {
        loadSymbolTable(section);
    }
}

// Load symbols from /usr/lib/debug/.build-id/ab/cdef1234.debug, where abcdef1234 is Build ID
bool ElfParser::loadSymbolsUsingBuildId() {
    ElfSection* section = findSection(SHT_NOTE, ".note.gnu.build-id");
    if (section == NULL || section->sh_size <= 16) {
        return false;
    }

    ElfNote* note = (ElfNote*)at(section);
    if (note->n_namesz != 4 || note->n_descsz < 2 || note->n_descsz > 64) {
        return false;
    }

    const char* build_id = (const char*)note + sizeof(*note) + 4;
    int build_id_len = note->n_descsz;

    char path[PATH_MAX];
    char* p = path + sprintf(path, "/usr/lib/debug/.build-id/%02hhx/", build_id[0]);
    for (int i = 1; i < build_id_len; i++) {
        p += sprintf(p, "%02hhx", build_id[i]);
    }
    strcpy(p, ".debug");

    return parseFile(_cc, _base, path, false);
}

// Look for debuginfo file specified in .gnu_debuglink section
bool ElfParser::loadSymbolsUsingDebugLink() {
    ElfSection* section = findSection(SHT_PROGBITS, ".gnu_debuglink");
    if (section == NULL || section->sh_size <= 4) {
        return false;
    }

    const char* basename = strrchr(_file_name, '/');
    if (basename == NULL) {
        return false;
    }

    char* dirname = strndup(_file_name, basename - _file_name);
    if (dirname == NULL) {
        return false;
    }

    const char* debuglink = at(section);
    char path[PATH_MAX];
    bool result = false;

    // 1. /path/to/libjvm.so.debug
    if (strcmp(debuglink, basename + 1) != 0 &&
        snprintf(path, sizeof(path), "%s/%s", dirname, debuglink) < sizeof(path)) {
        result = parseFile(_cc, _base, path, false);
    }

    // 2. /path/to/.debug/libjvm.so.debug
    if (!result && snprintf(path, sizeof(path), "%s/.debug/%s", dirname, debuglink) < sizeof(path)) {
        result = parseFile(_cc, _base, path, false);
    }

    // 3. /usr/lib/debug/path/to/libjvm.so.debug
    if (!result && snprintf(path, sizeof(path), "/usr/lib/debug%s/%s", dirname, debuglink) < sizeof(path)) {
        result = parseFile(_cc, _base, path, false);
    }

    free(dirname);
    return result;
}

void ElfParser::loadSymbolTable(ElfSection* symtab) {
    ElfSection* strtab = section(symtab->sh_link);
    const char* strings = at(strtab);

    const char* symbols = at(symtab);
    const char* symbols_end = symbols + symtab->sh_size;
    for (; symbols < symbols_end; symbols += symtab->sh_entsize) {
        ElfSymbol* sym = (ElfSymbol*)symbols;
        if (sym->st_name != 0 && sym->st_value != 0) {
            _cc->add(_base + sym->st_value, (int)sym->st_size, strings + sym->st_name);
        }
    }
}


void Symbols::parseKernelSymbols(NativeCodeCache* cc) {
    std::ifstream maps("/proc/kallsyms");
    std::string str;

    while (std::getline(maps, str)) {
        str += "_[k]";
        SymbolDesc symbol(str.c_str());
        char type = symbol.type();
        if (type == 'T' || type == 't' || type == 'V' || type == 'v' || type == 'W' || type == 'w') {
            const char* addr = symbol.addr();
            if (addr != NULL) {
                cc->add(addr, 0, symbol.name());
            }
        }
    }
}

int Symbols::parseMaps(NativeCodeCache** array, int size) {
    int count = 0;
    if (count < size) {
        NativeCodeCache* cc = new NativeCodeCache("[kernel]");
        parseKernelSymbols(cc);
        cc->sort();
        array[count++] = cc;
    }

    std::ifstream maps("/proc/self/maps");
    std::string str;

    while (count < size && std::getline(maps, str)) {
        MemoryMapDesc map(str.c_str());
        if (map.isExecutable() && map.file()[0] != 0) {
            NativeCodeCache* cc = new NativeCodeCache(map.file(), map.addr(), map.end());
            const char* base = map.addr() - map.offs();

            if (map.inode() != 0) {
                ElfParser::parseFile(cc, base, map.file(), true);
            } else if (strcmp(map.file(), "[vdso]") == 0) {
                ElfParser::parseMem(cc, base, base);
            }

            cc->sort();
            array[count++] = cc;
        }
    }

    return count;
}
