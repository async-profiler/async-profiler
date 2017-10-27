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

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include "symbols.h"


class MachOParser {
  private:
    NativeCodeCache* _cc;
    const char* _base;
    const char* _header;

    load_command* findCommand(uint32_t command) {
        mach_header_64* header = (mach_header_64*)_header;
        load_command* result = (load_command*)(_header + sizeof(mach_header_64));

        for (uint32_t i = 0; i < header->ncmds; i++) {
            if (result->cmd == command) {
                return result;
            }
            result = (load_command*)((uintptr_t)result + result->cmdsize);
        }

        return NULL;
    }

    void loadSymbols() {
        symtab_command* symtab = (symtab_command*)findCommand(LC_SYMTAB);
        if (symtab == NULL) {
            return;
        }

        nlist_64* symbol_table = (nlist_64*)(_header + symtab->symoff);
        const char* str_table = _header + symtab->stroff;

        for (uint32_t i = 0; i < symtab->nsyms; i++) {
            nlist_64 sym = symbol_table[i];
            if ((sym.n_type & 0xee) == 0x0e && sym.n_value != 0) {
                _cc->add(_base + sym.n_value, 0, str_table + sym.n_un.n_strx + 1);
            }
        }
    }

  public:
    MachOParser(NativeCodeCache* cc, const char* base, const char* header) : _cc(cc), _base(base), _header(header) {
    }

    static void parseFile(NativeCodeCache* cc, const char* base, const char* file_name) {
        int fd = open(file_name, O_RDONLY);
        if (fd == -1) {
            return;
        }

        size_t length = (size_t)lseek(fd, 0, SEEK_END);
        void* addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        if (addr != NULL) {
            MachOParser parser(cc, base, (const char*)addr);
            parser.loadSymbols();
            munmap(addr, length);
        }
    }
};


void Symbols::parseKernelSymbols(NativeCodeCache* cc) {
}

int Symbols::parseMaps(NativeCodeCache** array, int size) {
    int count = 0;
    uint32_t images = _dyld_image_count();

    for (uint32_t i = 0; i < images && count < size; i++) {
        const char* path = _dyld_get_image_name(i);
        const char* base = (const char*)_dyld_get_image_vmaddr_slide(i);

        // For now load only libjvm symbols. As soon as native stack traces
        // are supported on macOS, we'll take care about other native libraries
        size_t length = strlen(path);
        if (length >= 12 && strcmp(path + length - 12, "libjvm.dylib") == 0) {
            NativeCodeCache* cc = new NativeCodeCache(path);
            MachOParser::parseFile(cc, base, path);
            cc->sort();
            array[count++] = cc;
        }
    }

    return count;
}

#endif // __APPLE__
