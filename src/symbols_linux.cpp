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

#ifdef __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <elf.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <fstream>
#include <iostream>
#include <string>
#include "symbols.h"
#include "arch.h"


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
          _file = strchr(_inode, ' ');

          if (_file != NULL) {
              while (*_file == ' ') _file++;
          }
      }

      const char* file()    { return _file; }
      bool isExecutable()   { return _perm[0] == 'r' && _perm[2] == 'x'; }
      const char* addr()    { return (const char*)strtoul(_addr, NULL, 16); }
      const char* end()     { return (const char*)strtoul(_end, NULL, 16); }
      unsigned long offs()  { return strtoul(_offs, NULL, 16); }
      unsigned long inode() { return strtoul(_inode, NULL, 10); }
};


#ifdef __LP64__
const unsigned char ELFCLASS_SUPPORTED = ELFCLASS64;
typedef Elf64_Ehdr ElfHeader;
typedef Elf64_Shdr ElfSection;
typedef Elf64_Nhdr ElfNote;
typedef Elf64_Sym  ElfSymbol;
typedef Elf64_Rel  ElfRelocation;
#define ELF_R_SYM  ELF64_R_SYM
#else
const unsigned char ELFCLASS_SUPPORTED = ELFCLASS32;
typedef Elf32_Ehdr ElfHeader;
typedef Elf32_Shdr ElfSection;
typedef Elf32_Nhdr ElfNote;
typedef Elf32_Sym  ElfSymbol;
typedef Elf32_Rel  ElfRelocation;
#define ELF_R_SYM  ELF32_R_SYM
#endif // __LP64__


class ElfParser {
  private:
    NativeCodeCache* _cc;
    const char* _base;
    const char* _file_name;
    ElfHeader* _header;
    const char* _sections;

    ElfParser(NativeCodeCache* cc, const char* base, const void* addr, const char* file_name = NULL) {
        _cc = cc;
        _base = base;
        _file_name = file_name;
        _header = (ElfHeader*)addr;
        _sections = (const char*)addr + _header->e_shoff;
    }

    bool valid_header() {
        unsigned char* ident = _header->e_ident;
        return ident[0] == 0x7f && ident[1] == 'E' && ident[2] == 'L' && ident[3] == 'F'
            && ident[4] == ELFCLASS_SUPPORTED && ident[5] == ELFDATA2LSB && ident[6] == EV_CURRENT
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
    void addRelocationSymbols(ElfSection* reltab, const char* plt);

  public:
    static bool parseFile(NativeCodeCache* cc, const char* base, const char* file_name, bool use_debug);
    static void parseMem(NativeCodeCache* cc, const char* base);
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

    if (addr == MAP_FAILED) {
        if (strcmp(file_name, "/") == 0) {
            // https://bugs.launchpad.net/ubuntu/+source/linux/+bug/1843018
            fprintf(stderr, "Could not parse symbols due to the OS bug\n");
        } else {
            fprintf(stderr, "Could not parse symbols from %s: %s\n", file_name, strerror(errno));
        }
    } else {
        ElfParser elf(cc, base, addr, file_name);
        elf.loadSymbols(use_debug);
        munmap(addr, length);
    }
    return true;
}

void ElfParser::parseMem(NativeCodeCache* cc, const char* base) {
    ElfParser elf(cc, base, base);
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
        goto loaded;
    }

    // Try to load symbols from an external debuginfo library
    if (use_debug) {
        if (loadSymbolsUsingBuildId() || loadSymbolsUsingDebugLink()) {
            goto loaded;
        }
    }

    // If everything else fails, load only exported symbols
    section = findSection(SHT_DYNSYM, ".dynsym");
    if (section != NULL) {
        loadSymbolTable(section);
    }

loaded:
    // Synthesize names for PLT stubs
    if (use_debug) {
        ElfSection* plt = findSection(SHT_PROGBITS, ".plt");
        ElfSection* reltab = findSection(SHT_RELA, ".rela.plt");
        if (reltab == NULL) {
            reltab = findSection(SHT_REL, ".rel.plt");
        }
        if (plt != NULL && reltab != NULL) {
            addRelocationSymbols(reltab, _base + plt->sh_offset + PLT_HEADER_SIZE);
        }
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
        snprintf(path, PATH_MAX, "%s/%s", dirname, debuglink) < PATH_MAX) {
        result = parseFile(_cc, _base, path, false);
    }

    // 2. /path/to/.debug/libjvm.so.debug
    if (!result && snprintf(path, PATH_MAX, "%s/.debug/%s", dirname, debuglink) < PATH_MAX) {
        result = parseFile(_cc, _base, path, false);
    }

    // 3. /usr/lib/debug/path/to/libjvm.so.debug
    if (!result && snprintf(path, PATH_MAX, "/usr/lib/debug%s/%s", dirname, debuglink) < PATH_MAX) {
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

void ElfParser::addRelocationSymbols(ElfSection* reltab, const char* plt) {
    ElfSection* symtab = section(reltab->sh_link);
    const char* symbols = at(symtab);

    ElfSection* strtab = section(symtab->sh_link);
    const char* strings = at(strtab);

    const char* relocations = at(reltab);
    const char* relocations_end = relocations + reltab->sh_size;
    for (; relocations < relocations_end; relocations += reltab->sh_entsize) {
        ElfRelocation* r = (ElfRelocation*)relocations;
        ElfSymbol* sym = (ElfSymbol*)(symbols + ELF_R_SYM(r->r_info) * symtab->sh_entsize);

        char name[256];
        if (sym->st_name == 0) {
            strcpy(name, "@plt");
        } else {
            const char* sym_name = strings + sym->st_name;
            snprintf(name, sizeof(name), "%s%cplt", sym_name, sym_name[0] == '_' && sym_name[1] == 'Z' ? '.' : '@');
            name[sizeof(name) - 1] = 0;
        }

        _cc->add(plt, PLT_ENTRY_SIZE, name);
        plt += PLT_ENTRY_SIZE;
    }
}


Mutex Symbols::_parse_lock;
std::set<const void*> Symbols::_parsed_libraries;
bool Symbols::_have_kernel_symbols = false;

void Symbols::parseKernelSymbols(NativeCodeCache* cc) {
    std::ifstream maps("/proc/kallsyms");
    std::string str;

    while (std::getline(maps, str)) {
        str += "_[k]";
        SymbolDesc symbol(str.c_str());
        char type = symbol.type();
        if (type == 'T' || type == 't' || type == 'W' || type == 'w') {
            const char* addr = symbol.addr();
            if (addr != NULL) {
                cc->add(addr, 0, symbol.name());
                _have_kernel_symbols = true;
            }
        }
    }
}

void Symbols::parseLibraries(NativeCodeCache** array, volatile int& count, int size) {
    MutexLocker ml(_parse_lock);

    if (!haveKernelSymbols()) {
        NativeCodeCache* cc = new NativeCodeCache("[kernel]");
        parseKernelSymbols(cc);

        if (haveKernelSymbols()) {
            cc->sort();
            array[count] = cc;
            atomicInc(count);
        } else {
            delete cc;
        }
    }

    std::ifstream maps("/proc/self/maps");
    std::string str;

    while (count < size && std::getline(maps, str)) {
        MemoryMapDesc map(str.c_str());
        if (map.isExecutable() && map.file() != NULL && map.file()[0] != 0) {
            const char* image_base = map.addr();
            if (!_parsed_libraries.insert(image_base).second) {
                continue;  // the library was already parsed
            }

            NativeCodeCache* cc = new NativeCodeCache(map.file(), image_base, map.end());

            if (map.inode() != 0) {
                ElfParser::parseFile(cc, image_base - map.offs(), map.file(), true);
            } else if (strcmp(map.file(), "[vdso]") == 0) {
                ElfParser::parseMem(cc, image_base);
            }

            cc->sort();
            array[count] = cc;
            atomicInc(count);
        }
    }
}

#endif // __linux__
