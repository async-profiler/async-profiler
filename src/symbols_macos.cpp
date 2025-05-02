/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __APPLE__

#include <unordered_set>
#include <dlfcn.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include "symbols.h"
#include "log.h"
#include <vector>

#ifndef SEG_DATA_CONST
#define SEG_DATA_CONST  "__DATA_CONST"
#endif

class MachOParser {
  private:
    CodeCache* _cc;
    const mach_header* _image_base;
    intptr_t _slide;

    static const char* add(const void* base, uint64_t offset) {
        return (const char*)base + offset;
    }

    void findSymbolPtrSection(const segment_command_64* sc, std::vector<const section_64*>* symbol_sections) {
        const section_64* section = (const section_64*)add(sc, sizeof(segment_command_64));
        for (uint32_t i = 0; i < sc->nsects; i++) {
            if ((section->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS || (section->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS) {
                symbol_sections->push_back(section);
            }
            section++;
        }
    }

    void loadSymbols(const symtab_command* symtab, const char* text_base, const char* link_base) {
        const nlist_64* sym = (const nlist_64*)add(link_base, symtab->symoff);
        const char* str_table = add(link_base, symtab->stroff);
        bool debug_symbols = false;

        for (uint32_t i = 0; i < symtab->nsyms; i++) {
            if ((sym->n_type & 0xee) == 0x0e && sym->n_value != 0) {
                const char* addr = text_base + sym->n_value;
                const char* name = str_table + sym->n_un.n_strx;
                if (name[0] == '_') name++;
                _cc->add(addr, 0, name);
                debug_symbols = true;
            }
            sym++;
        }

        _cc->setDebugSymbols(debug_symbols);
    }

    void loadImports(const section_64* section, intptr_t slide, nlist_64 *symtab, char *strtab, uint32_t *indirect_symtab) {
        uint32_t *indirect_symbol_indices = indirect_symtab + section->reserved1;
        void **indirect_symbol_bindings = (void **)((uintptr_t)slide + section->addr);

        for (uint i = 0; i < section->size / sizeof(void *); i++) {
            uint32_t symtab_index = indirect_symbol_indices[i];
            if (symtab_index == INDIRECT_SYMBOL_ABS || symtab_index == INDIRECT_SYMBOL_LOCAL || symtab_index == (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)){
                continue;
            }
            uint32_t strtab_offset = symtab[symtab_index].n_un.n_strx;
            char *symbol_name = strtab + strtab_offset;
            bool symbol_name_longer_than_1 = symbol_name[0] && symbol_name[1];

            if (symbol_name_longer_than_1) {
                _cc->addImport(&indirect_symbol_bindings[i], symbol_name+1);
            }
        }
    }

  public:
    MachOParser(CodeCache* cc, const mach_header* image_base, intptr_t slide) : _cc(cc), _image_base(image_base), _slide(slide) {
    }

    bool parse() {
        if (_image_base->magic != MH_MAGIC_64) {
            return false;
        }

        const mach_header_64* header = (const mach_header_64*)_image_base;
        const load_command* lc = (const load_command*)(header + 1);

        const char* UNDEFINED = (const char*)-1;
        const char* text_base = UNDEFINED;
        const char* link_base = UNDEFINED;
        const symtab_command* symtab_cmd = NULL;
        const dysymtab_command* dysymtab_cmd = NULL;
        const segment_command_64* linkedit_segment = NULL;
        bool data_const_section = false;
        std::vector<const section_64*> symbol_sections;

        for (uint32_t i = 0; i < header->ncmds; i++) {
            if (lc->cmd == LC_SEGMENT_64) {
                const segment_command_64* sc = (const segment_command_64*)lc;
                if (strcmp(sc->segname, SEG_TEXT) == 0) {
                    text_base = (const char*)_image_base - sc->vmaddr;
                    _cc->setTextBase(text_base);
                    _cc->updateBounds(_image_base, add(_image_base, sc->vmsize));
                } else if (strcmp(sc->segname, SEG_LINKEDIT) == 0) {
                    linkedit_segment = sc;
                    link_base = text_base + sc->vmaddr - sc->fileoff;
                } else if (strcmp(sc->segname, SEG_DATA_CONST) == 0 || strcmp(sc->segname, SEG_DATA) == 0) {
                    findSymbolPtrSection(sc, &symbol_sections);
                }
            } else if (lc->cmd == LC_SYMTAB) {
                symtab_cmd = (const symtab_command*)lc;
            } else if (lc->cmd == LC_DYSYMTAB) {
                dysymtab_cmd = (const dysymtab_command*)lc;
            }
            lc = (const load_command*)add(lc, lc->cmdsize);
        }

        // Parse Debug symbols
        if (text_base != UNDEFINED && link_base != UNDEFINED) {
            loadSymbols(symtab_cmd, text_base, link_base);
        }

        // Can't load imports from library
        if (!symtab_cmd || !dysymtab_cmd || !linkedit_segment || !dysymtab_cmd->nindirectsyms || symbol_sections.size() == 0) {
          return true;
        }

        uintptr_t linkedit_base = (uintptr_t)this->_slide + linkedit_segment->vmaddr - linkedit_segment->fileoff;
        nlist_64 *symtab = (nlist_64 *)(linkedit_base + symtab_cmd->symoff);
        char *strtab = (char *)(linkedit_base + symtab_cmd->stroff);

        // Get indirect symbol table (array of uint32_t indices into symbol table)
        uint32_t *indirect_symtab = (uint32_t *)(linkedit_base + dysymtab_cmd->indirectsymoff);

        // Load imports for library
        for (auto symbol_section : symbol_sections) {
            loadImports(symbol_section, this->_slide, symtab, strtab, indirect_symtab);
        }
        return true;
    }
};


Mutex Symbols::_parse_lock;
bool Symbols::_have_kernel_symbols = false;
bool Symbols::_libs_limit_reported = false;
static std::unordered_set<const void*> _parsed_libraries;

void Symbols::parseKernelSymbols(CodeCache* cc) {
}

void Symbols::parseLibraries(CodeCacheArray* array, bool kernel_symbols) {
    MutexLocker ml(_parse_lock);
    uint32_t images = _dyld_image_count();

    for (uint32_t i = 0; i < images; i++) {
        const mach_header* image_base = _dyld_get_image_header(i);
        intptr_t slide = _dyld_get_image_vmaddr_slide(i);
        const char* path = _dyld_get_image_name(i);

        if (image_base == NULL || !_parsed_libraries.insert(image_base).second) {
            continue;  // the library was already parsed
        }

        int count = array->count();
        if (count >= MAX_NATIVE_LIBS) {
            if (!_libs_limit_reported) {
                Log::warn("Number of parsed libraries reached the limit of %d", MAX_NATIVE_LIBS);
                _libs_limit_reported = true;
            }
            break;
        }

        // Protect the library from unloading while parsing symbols
        void* handle = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
        if (handle == NULL) {
            continue;
        }

        CodeCache* cc = new CodeCache(path, count);
        MachOParser parser(cc, image_base, slide);
        if (!parser.parse()) {
            Log::warn("Could not parse symbols from %s", path);
        }
        dlclose(handle);

        cc->sort();
        array->add(cc);
    }
}

#endif // __APPLE__
