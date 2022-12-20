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

#include <set>
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
#include "symbols.h"
#include "dwarf.h"
#include "fdtransferClient.h"
#include "log.h"


class SymbolDesc {
  private:
    const char* _addr;
    const char* _desc;

  public:
      SymbolDesc(const char* s) {
          _addr = s;
          _desc = strchr(_addr, ' ');
      }

      const char* addr() { return (const char*)strtoul(_addr, NULL, 16); }
      char type()        { return _desc != NULL ? _desc[1] : 0; }
      const char* name() { return _desc + 3; }
};

class MemoryMapDesc {
  private:
    const char* _addr;
    const char* _end;
    const char* _perm;
    const char* _offs;
    const char* _dev;
    const char* _inode;
    const char* _file;

  public:
      MemoryMapDesc(const char* s) {
          _addr = s;
          _end = strchr(_addr, '-') + 1;
          _perm = strchr(_end, ' ') + 1;
          _offs = strchr(_perm, ' ') + 1;
          _dev = strchr(_offs, ' ') + 1;
          _inode = strchr(_dev, ' ') + 1;
          _file = strchr(_inode, ' ');

          if (_file != NULL) {
              while (*_file == ' ') _file++;
          }
      }

      const char* file()    { return _file; }
      bool isReadable()     { return _perm[0] == 'r'; }
      bool isExecutable()   { return _perm[2] == 'x'; }
      const char* addr()    { return (const char*)strtoul(_addr, NULL, 16); }
      const char* end()     { return (const char*)strtoul(_end, NULL, 16); }
      unsigned long offs()  { return strtoul(_offs, NULL, 16); }
      unsigned long inode() { return strtoul(_inode, NULL, 10); }

      unsigned long dev() {
          char* colon;
          unsigned long major = strtoul(_dev, &colon, 16);
          unsigned long minor = strtoul(colon + 1, NULL, 16);
          return major << 8 | minor;
      }
};


#ifdef __LP64__
const unsigned char ELFCLASS_SUPPORTED = ELFCLASS64;
typedef Elf64_Ehdr ElfHeader;
typedef Elf64_Shdr ElfSection;
typedef Elf64_Phdr ElfProgramHeader;
typedef Elf64_Nhdr ElfNote;
typedef Elf64_Sym  ElfSymbol;
typedef Elf64_Rel  ElfRelocation;
typedef Elf64_Dyn  ElfDyn;
#define ELF_R_TYPE ELF64_R_TYPE
#define ELF_R_SYM  ELF64_R_SYM
#else
const unsigned char ELFCLASS_SUPPORTED = ELFCLASS32;
typedef Elf32_Ehdr ElfHeader;
typedef Elf32_Shdr ElfSection;
typedef Elf32_Phdr ElfProgramHeader;
typedef Elf32_Nhdr ElfNote;
typedef Elf32_Sym  ElfSymbol;
typedef Elf32_Rel  ElfRelocation;
typedef Elf32_Dyn  ElfDyn;
#define ELF_R_TYPE ELF32_R_TYPE
#define ELF_R_SYM  ELF32_R_SYM
#endif // __LP64__

#if defined(__x86_64__)
#  define R_GLOB_DAT R_X86_64_GLOB_DAT
#elif defined(__i386__)
#  define R_GLOB_DAT R_386_GLOB_DAT
#elif defined(__arm__) || defined(__thumb__)
#  define R_GLOB_DAT R_ARM_GLOB_DAT
#elif defined(__aarch64__)
#  define R_GLOB_DAT R_AARCH64_GLOB_DAT
#elif defined(__PPC64__)
#  define R_GLOB_DAT R_PPC64_GLOB_DAT
#else
#  error "Compiling on unsupported arch"
#endif

// GNU dynamic linker relocates pointers in the dynamic section, while musl doesn't.
// A tricky case is when we attach to a musl container from a glibc host.
#ifdef __musl__
#  define DYN_PTR(ptr)  (_base + (ptr))
#else
#  define DYN_PTR(ptr)  ((char*)(ptr) >= _base ? (char*)(ptr) : _base + (ptr))
#endif // __musl__


class ElfParser {
  private:
    CodeCache* _cc;
    const char* _base;
    const char* _file_name;
    ElfHeader* _header;
    const char* _sections;
    const char* _vaddr_diff;

    ElfParser(CodeCache* cc, const char* base, const void* addr, const char* file_name = NULL) {
        _cc = cc;
        _base = base;
        _file_name = file_name;
        _header = (ElfHeader*)addr;
        _sections = (const char*)addr + _header->e_shoff;
    }

    bool validHeader() {
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

    const char* at(ElfProgramHeader* pheader) {
        return _header->e_type == ET_EXEC ? (const char*)pheader->p_vaddr : _vaddr_diff + pheader->p_vaddr;
    }

    ElfSection* findSection(uint32_t type, const char* name);
    ElfProgramHeader* findProgramHeader(uint32_t type);

    void calcVirtualLoadAddress();
    void parseDynamicSection();
    void parseDwarfInfo();
    void loadSymbols(bool use_debug);
    bool loadSymbolsUsingBuildId();
    bool loadSymbolsUsingDebugLink();
    void loadSymbolTable(ElfSection* symtab);
    void addRelocationSymbols(ElfSection* reltab, const char* plt);

  public:
    static void parseProgramHeaders(CodeCache* cc, const char* base, const char* end);
    static bool parseFile(CodeCache* cc, const char* base, const char* file_name, bool use_debug);
    static void parseMem(CodeCache* cc, const char* base);
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

ElfProgramHeader* ElfParser::findProgramHeader(uint32_t type) {
    const char* pheaders = (const char*)_header + _header->e_phoff;

    for (int i = 0; i < _header->e_phnum; i++) {
        ElfProgramHeader* pheader = (ElfProgramHeader*)(pheaders + i * _header->e_phentsize);
        if (pheader->p_type == type) {
            return pheader;
        }
    }

    return NULL;
}

bool ElfParser::parseFile(CodeCache* cc, const char* base, const char* file_name, bool use_debug) {
    int fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    size_t length = (size_t)lseek64(fd, 0, SEEK_END);
    void* addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        Log::warn("Could not parse symbols from %s: %s", file_name, strerror(errno));
    } else {
        ElfParser elf(cc, base, addr, file_name);
        if (elf.validHeader()) {
            elf.loadSymbols(use_debug);
        }
        munmap(addr, length);
    }
    return true;
}

void ElfParser::parseMem(CodeCache* cc, const char* base) {
    ElfParser elf(cc, base, base);
    if (elf.validHeader()) {
        elf.loadSymbols(false);
    }
}

void ElfParser::parseProgramHeaders(CodeCache* cc, const char* base, const char* end) {
    ElfParser elf(cc, base, base);
    if (elf.validHeader() && base + elf._header->e_phoff < end) {
        cc->setTextBase(base);
        elf.calcVirtualLoadAddress();
        elf.parseDynamicSection();
        elf.parseDwarfInfo();
    }
}

void ElfParser::calcVirtualLoadAddress() {
    // Find a difference between the virtual load address (often zero) and the actual DSO base
    const char* pheaders = (const char*)_header + _header->e_phoff;
    for (int i = 0; i < _header->e_phnum; i++) {
        ElfProgramHeader* pheader = (ElfProgramHeader*)(pheaders + i * _header->e_phentsize);
        if (pheader->p_type == PT_LOAD) {
            _vaddr_diff = _base - pheader->p_vaddr;
            return;
        }
    }
    _vaddr_diff = _base;
}

void ElfParser::parseDynamicSection() {
    ElfProgramHeader* dynamic = findProgramHeader(PT_DYNAMIC);
    if (dynamic != NULL) {
        void** got_start = NULL;
        size_t pltrelsz = 0;
        char* rel = NULL;
        size_t relsz = 0;
        size_t relent = 0;
        size_t relcount = 0;

        const char* dyn_start = at(dynamic);
        const char* dyn_end = dyn_start + dynamic->p_memsz;
        for (ElfDyn* dyn = (ElfDyn*)dyn_start; dyn < (ElfDyn*)dyn_end; dyn++) {
            switch (dyn->d_tag) {
                case DT_PLTGOT:
                    got_start = (void**)DYN_PTR(dyn->d_un.d_ptr) + 3;
                    break;
                case DT_PLTRELSZ:
                    pltrelsz = dyn->d_un.d_val;
                    break;
                case DT_RELA:
                case DT_REL:
                    rel = (char*)DYN_PTR(dyn->d_un.d_ptr);
                    break;
                case DT_RELASZ:
                case DT_RELSZ:
                    relsz = dyn->d_un.d_val;
                    break;
                case DT_RELAENT:
                case DT_RELENT:
                    relent = dyn->d_un.d_val;
                    break;
                case DT_RELACOUNT:
                case DT_RELCOUNT:
                    relcount = dyn->d_un.d_val;
                    break;
            }
        }

        if (relent != 0) {
            if (pltrelsz != 0 && got_start != NULL) {
                // The number of entries in .got.plt section matches the number of entries in .rela.plt
                _cc->setGlobalOffsetTable(got_start, got_start + pltrelsz / relent, false);
            } else if (rel != NULL && relsz != 0) {
                // RELRO technique: .got.plt has been merged into .got and made read-only.
                // Find .got end from the highest relocation address.
                void** min_addr = (void**)-1;
                void** max_addr = (void**)0;
                for (size_t offs = relcount * relent; offs < relsz; offs += relent) {
                    ElfRelocation* r = (ElfRelocation*)(rel + offs);
                    if (ELF_R_TYPE(r->r_info) == R_GLOB_DAT) {
                        void** addr = (void**)(_base + r->r_offset);
                        if (addr < min_addr) min_addr = addr;
                        if (addr > max_addr) max_addr = addr;
                    }
                }

                if (got_start == NULL) {
                    got_start = (void**)min_addr;
                }

                if (max_addr >= got_start) {
                    _cc->setGlobalOffsetTable(got_start, max_addr + 1, false);
                }
            }
        }
    }
}

void ElfParser::parseDwarfInfo() {
    if (!DWARF_SUPPORTED) return;

    ElfProgramHeader* eh_frame_hdr = findProgramHeader(PT_GNU_EH_FRAME);
    if (eh_frame_hdr != NULL) {
        DwarfParser dwarf(_cc->name(), _base, at(eh_frame_hdr));
        _cc->setDwarfTable(dwarf.table(), dwarf.count());
    }
}

void ElfParser::loadSymbols(bool use_debug) {
    // Look for debug symbols in the original .so
    ElfSection* section = findSection(SHT_SYMTAB, ".symtab");
    if (section != NULL) {
        loadSymbolTable(section);
        _cc->setDebugSymbols(true);
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
    if (use_debug) {
        // Synthesize names for PLT stubs
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
            // Skip special AArch64 mapping symbols: $x and $d
            if (sym->st_size != 0 || sym->st_info != 0 || strings[sym->st_name] != '$') {
                _cc->add(_base + sym->st_value, (int)sym->st_size, strings + sym->st_name);
            }
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
bool Symbols::_have_kernel_symbols = false;
static std::set<const void*> _parsed_libraries;
static std::set<u64> _parsed_inodes;

void Symbols::parseKernelSymbols(CodeCache* cc) {
    int fd;
    if (FdTransferClient::hasPeer()) {
        fd = FdTransferClient::requestKallsymsFd();
    } else {
        fd = open("/proc/kallsyms", O_RDONLY);
    }

    if (fd == -1) {
        Log::warn("open(\"/proc/kallsyms\"): %s", strerror(errno));
        return;
    }

    FILE* f = fdopen(fd, "r");
    if (f == NULL) {
        Log::warn("fdopen(): %s", strerror(errno));
        close(fd);
        return;
    }

    char str[256];
    while (fgets(str, sizeof(str) - 8, f) != NULL) {
        size_t len = strlen(str) - 1; // trim the '\n'
        strcpy(str + len, "_[k]");

        SymbolDesc symbol(str);
        char type = symbol.type();
        if (type == 'T' || type == 't' || type == 'W' || type == 'w') {
            const char* addr = symbol.addr();
            if (addr != NULL) {
                if (!_have_kernel_symbols) {
                    if (strncmp(symbol.name(), "__LOAD_PHYSICAL_ADDR", 20) == 0 ||
                        strncmp(symbol.name(), "phys_startup", 12) == 0) {
                        continue;
                    }
                    _have_kernel_symbols = true;
                }
                cc->add(addr, 0, symbol.name());
            }
        }
    }

    fclose(f);
}

void Symbols::parseLibraries(CodeCacheArray* array, bool kernel_symbols) {
    MutexLocker ml(_parse_lock);

    if (kernel_symbols && !haveKernelSymbols()) {
        CodeCache* cc = new CodeCache("[kernel]");
        parseKernelSymbols(cc);

        if (haveKernelSymbols()) {
            cc->sort();
            array->add(cc);
        } else {
            delete cc;
        }
    }

    FILE* f = fopen("/proc/self/maps", "r");
    if (f == NULL) {
        return;
    }

    const char* last_readable_base = NULL;
    const char* image_end = NULL;
    char* str = NULL;
    size_t str_size = 0;
    ssize_t len;

    while ((len = getline(&str, &str_size, f)) > 0) {
        str[len - 1] = 0;

        MemoryMapDesc map(str);
        if (!map.isReadable() || map.file() == NULL || map.file()[0] == 0) {
            continue;
        }
        if (strchr(map.file(), ':') != NULL) {
            // Skip pseudofiles like anon_inode:name, /memfd:name
            continue;
        }

        const char* image_base = map.addr();
        if (image_base != image_end) last_readable_base = image_base;
        image_end = map.end();

        if (map.isExecutable()) {
            if (!_parsed_libraries.insert(image_base).second) {
                continue;  // the library was already parsed
            }

            int count = array->count();
            if (count >= MAX_NATIVE_LIBS) {
                break;
            }

            CodeCache* cc = new CodeCache(map.file(), count, image_base, image_end);

            unsigned long inode = map.inode();
            if (inode != 0) {
                // Do not parse the same executable twice, e.g. on Alpine Linux
                if (_parsed_inodes.insert(u64(map.dev()) << 32 | inode).second) {
                    // Be careful: executable file is not always ELF, e.g. classes.jsa
                    unsigned long offs = map.offs();
                    if ((unsigned long)image_base > offs && (image_base -= offs) >= last_readable_base) {
                        ElfParser::parseProgramHeaders(cc, image_base, image_end);
                    }
                    ElfParser::parseFile(cc, image_base, map.file(), true);
                }
            } else if (strcmp(map.file(), "[vdso]") == 0) {
                ElfParser::parseMem(cc, image_base);
            }

            cc->sort();
            array->add(cc);
        }
    }

    free(str);
    fclose(f);
}

#endif // __linux__
