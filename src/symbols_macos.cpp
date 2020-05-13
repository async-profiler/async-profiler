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

#ifdef __APPLE__

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/nlist.h>
#include "symbols.h"
#include "arch.h"


class MachOParser {
  private:
    NativeCodeCache* _cc;
    const mach_header* _image_base;

    static const char* add(const void* base, uint32_t offset) {
        return (const char*)base + offset;
    }

    void loadSymbols(mach_header_64* header, symtab_command* symtab) {
        nlist_64* symbol_table = (nlist_64*)add(header, symtab->symoff);
        const char* str_table = add(header, symtab->stroff);

        for (uint32_t i = 0; i < symtab->nsyms; i++) {
            nlist_64 sym = symbol_table[i];
            if ((sym.n_type & 0xee) == 0x0e && sym.n_value != 0) {
                const char* addr = add(_image_base, sym.n_value);
                const char* name = str_table + sym.n_un.n_strx;
                if (name[0] == '_') name++;
                _cc->add(addr, 0, name);
            }
        }
    }

    void parseMachO(mach_header_64* header) {
        load_command* lc = (load_command*)(header + 1);

        for (uint32_t i = 0; i < header->ncmds; i++) {
            if (lc->cmd == LC_SYMTAB) {
                loadSymbols(header, (symtab_command*)lc);
                break;
            }
            lc = (load_command*)add(lc, lc->cmdsize);
        }
    }

    void parseFatObject(fat_header* header) {
        int narch = header->nfat_arch;
        fat_arch* arch = (fat_arch*)(header + 1);

        for (uint32_t i = 0; i < narch; i++) {
            if (arch[i].cputype == _image_base->cputype &&
                arch[i].cpusubtype == _image_base->cpusubtype) {
                parseMachO((mach_header_64*)add(header, arch[i].offset));
            }
        }
    }

    // The same as parseFatObject, but fields are big-endian
    void parseFatObjectBE(fat_header* header) {
        int narch = htonl(header->nfat_arch);
        fat_arch* arch = (fat_arch*)(header + 1);

        for (uint32_t i = 0; i < narch; i++) {
            if (htonl(arch[i].cputype) == _image_base->cputype &&
                htonl(arch[i].cpusubtype) == _image_base->cpusubtype) {
                parseMachO((mach_header_64*)add(header, htonl(arch[i].offset)));
            }
        }
    }

    void parse(mach_header* header) {
        uint32_t magic = header->magic;
        if (magic == MH_MAGIC_64) {
            parseMachO((mach_header_64*)header);
        } else if (magic == FAT_MAGIC) {
            parseFatObject((fat_header*)header);
        } else if (magic == FAT_CIGAM) {
            parseFatObjectBE((fat_header*)header);
        }
    }

  public:
    MachOParser(NativeCodeCache* cc, const mach_header* image_base) : _cc(cc), _image_base(image_base) {
    }

    static void parseFile(NativeCodeCache* cc, const mach_header* image_base, const char* file_name) {
        int fd = open(file_name, O_RDONLY);
        if (fd == -1) {
            return;
        }

        size_t length = (size_t)lseek(fd, 0, SEEK_END);
        void* addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        if (addr == MAP_FAILED) {
            fprintf(stderr, "Could not parse symbols from %s: %s\n", file_name, strerror(errno));
        } else {
            MachOParser parser(cc, image_base);
            parser.parse((mach_header*)addr);
            munmap(addr, length);
        }
    }
};


Mutex Symbols::_parse_lock;
std::set<const void*> Symbols::_parsed_libraries;
bool Symbols::_have_kernel_symbols = false;

void Symbols::parseKernelSymbols(NativeCodeCache* cc) {
}

void Symbols::parseLibraries(NativeCodeCache** array, volatile int& count, int size) {
    MutexLocker ml(_parse_lock);
    uint32_t images = _dyld_image_count();

    for (uint32_t i = 0; i < images && count < size; i++) {
        const mach_header* image_base = _dyld_get_image_header(i);
        if (!_parsed_libraries.insert(image_base).second) {
            continue;  // the library was already parsed
        }

        const char* path = _dyld_get_image_name(i);

        NativeCodeCache* cc = new NativeCodeCache(path);
        MachOParser::parseFile(cc, image_base, path);

        cc->sort();
        array[count] = cc;
        atomicInc(count);
    }
}

#endif // __APPLE__
