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

#include <mach-o/loader.h>
#include <cstring>
#include <mach-o/nlist.h>
#include "symbols.h"
#include <sys/mman.h>
#include <mach-o/dyld_images.h>
#include <fcntl.h>

// Workaround for newer macosx versions: since 10.12.x _dyld_get_all_image_infos exists, but is not exported
extern "C" const struct dyld_all_image_infos* _dyld_get_all_image_infos();

class MachOParser {
  private:
    void* _lib_addr;

    static load_command* find_command(mach_header_64* header, uint32_t command) {
        load_command* result = (load_command*)((uint64_t)header + sizeof(mach_header_64));

        if (command == 0) {
            return result;
        }

        for (uint32_t i = 0; i < header->ncmds; i++) {
            if (result->cmd == command) {
                break;
            }

            result = (load_command*)((uint64_t)result + result->cmdsize);
        }
        return result;
    }

  public:
    MachOParser(void* _lib_addr) : _lib_addr(_lib_addr) {
    }

    uintptr_t find_symbol_offset(const char* symbol_name) {
        load_command* command = find_command((mach_header_64*)_lib_addr, LC_SYMTAB);
        symtab_command* symtab = (symtab_command*)command;
        nlist_64* symbol_table = (nlist_64*)((char*)_lib_addr + symtab->symoff);

        const char* str_table = (char*)_lib_addr + symtab->stroff;
        // Scan through whole symbol table
        for (uint32_t sym_n = 0; sym_n < symtab->nsyms; ++sym_n) {
            uint32_t str_offset = symbol_table[sym_n].n_un.n_strx;
            const char* sym_name = &str_table[str_offset];
            uintptr_t offset = symbol_table[sym_n].n_value;
            if (strcmp(sym_name, symbol_name) == 0 && offset != 0) {
                return offset;
            }
        }
        return 0;
    }
};

void add_symbol(MachOParser &parser, uintptr_t lib_addr, const char* symbol_name, NativeCodeCache* cc) {
    uintptr_t offset = parser.find_symbol_offset(symbol_name);
    cc->add((void*)(lib_addr + offset), sizeof(uintptr_t), symbol_name + 1);
}

void Symbols::parseKernelSymbols(NativeCodeCache* cc) {
}

int Symbols::parseMaps(NativeCodeCache** array, int size) {

    const dyld_all_image_infos* dyld_all_image_infos = _dyld_get_all_image_infos();
    const char* lib_path = NULL;
    uintptr_t loaded_lib_addr = 0;
    for (int i = 0; i < dyld_all_image_infos->infoArrayCount; i++) {
        const char* path = dyld_all_image_infos->infoArray[i].imageFilePath;
        size_t length = strlen(path);
        if (strcmp(path + length - 12, "libjvm.dylib") == 0) {
            lib_path = path;
            loaded_lib_addr = (uintptr_t)dyld_all_image_infos->infoArray[i].imageLoadAddress;
        }
    }

    if (lib_path == NULL) {
        return 0;
    }

    // Loaded lib has different symbol table representation, it's much easier to parse original one
    int fd = open(lib_path, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    size_t length = (size_t)lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    uint32_t magic_number;
    read(fd, &magic_number, sizeof(magic_number));
    if (magic_number != MH_MAGIC_64 && magic_number != MH_CIGAM_64) {
        close(fd);
        return 0;
    }

    void* mapped_lib_addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (mapped_lib_addr == MAP_FAILED) {
        return 0;
    }

    MachOParser parser(mapped_lib_addr);

    NativeCodeCache* cc = new NativeCodeCache(lib_path);
    array[0] = cc;

    // There is a lot of private symbols in libjvm, register only required ones (until native stack walking is implemented)
    add_symbol(parser, loaded_lib_addr, "_AsyncGetCallTrace", cc);

    add_symbol(parser, loaded_lib_addr, "_gHotSpotVMStructs", cc);
    add_symbol(parser, loaded_lib_addr, "_gHotSpotVMStructEntryArrayStride", cc);
    add_symbol(parser, loaded_lib_addr, "_gHotSpotVMStructEntryTypeNameOffset", cc);
    add_symbol(parser, loaded_lib_addr, "_gHotSpotVMStructEntryFieldNameOffset", cc);
    add_symbol(parser, loaded_lib_addr, "_gHotSpotVMStructEntryOffsetOffset", cc);

    add_symbol(parser, loaded_lib_addr, "__ZN11AllocTracer33send_allocation_in_new_tlab_eventE11KlassHandlemm", cc);
    add_symbol(parser, loaded_lib_addr, "__ZN11AllocTracer34send_allocation_outside_tlab_eventE11KlassHandlem", cc);
    munmap(mapped_lib_addr, length);
    cc->sort();
    return 1;
}

#endif // __APPLE__
