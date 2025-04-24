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

UnloadProtection::UnloadProtection(const CodeCache *cc) {
    // Protect library from unloading while parsing in-memory ELF program headers.
    // Also, dlopen() ensures the library is fully loaded.
    _lib_handle = dlopen(cc->name(), RTLD_LAZY | RTLD_NOLOAD);
    _valid = _lib_handle != NULL;
}

UnloadProtection::~UnloadProtection() {
    if (_lib_handle != NULL) {
        dlclose(_lib_handle);
    }
}

class MachOParser {
  private:
    CodeCache* _cc;
    const mach_header* _image_base;

    static const char* add(const void* base, uint64_t offset) {
        return (const char*)base + offset;
    }

    void findSymbolPtrSection(const segment_command_64* sc, std::vector<const section_64*>& symbol_sections) {
        const section_64* section = (const section_64*)add(sc, sizeof(segment_command_64));
        for (uint32_t i = 0; i < sc->nsects; i++) {
            uint32_t section_type = section->flags & SECTION_TYPE;
            if (section_type == S_LAZY_SYMBOL_POINTERS || section_type == S_NON_LAZY_SYMBOL_POINTERS) {
                symbol_sections.push_back(section);
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

    void loadImports(const section_64* symbol_section, const char* text_base, const nlist_64* symbols,
                     const char* string_table, const uint32_t* dynamic_symbols) {
        const uint32_t* dynamic_symbol_indices = dynamic_symbols + symbol_section->reserved1;
        void** dynamic_symbol_references = (void**)((uintptr_t)text_base + symbol_section->addr);

        for (uint64_t i = 0; i < symbol_section->size / sizeof(void*); i++) {
            const uint32_t dynamic_symbol_index = dynamic_symbol_indices[i];
            if (dynamic_symbol_index == INDIRECT_SYMBOL_ABS || dynamic_symbol_index == INDIRECT_SYMBOL_LOCAL
                    || dynamic_symbol_index == (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
                continue;
            }
            const char* symbol_name = string_table + symbols[dynamic_symbol_index].n_un.n_strx;
            if (symbol_name[0] == '_' && symbol_name[1] != '\0') {
                // first character in symbol name is always '_' so it's skipped
                _cc->addImport(&dynamic_symbol_references[i], symbol_name + 1);
            }
        }
    }

  public:
    MachOParser(CodeCache* cc, const mach_header* image_base) : _cc(cc), _image_base(image_base) {
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
        const symtab_command* symbol_table = NULL;
        const dysymtab_command* dynamic_symbol_table = NULL;
        std::vector<const section_64*> symbol_sections;

        for (uint32_t i = 0; i < header->ncmds; i++) {
            if (lc->cmd == LC_SEGMENT_64) {
                const segment_command_64* sc = (const segment_command_64*)lc;
                if (strcmp(sc->segname, SEG_TEXT) == 0) {
                    text_base = (const char*)_image_base - sc->vmaddr;
                    _cc->setTextBase(text_base);
                    _cc->updateBounds(_image_base, add(_image_base, sc->vmsize));
                } else if (strcmp(sc->segname, SEG_LINKEDIT) == 0) {
                    link_base = text_base + sc->vmaddr - sc->fileoff;
                } else if (strcmp(sc->segname, "__DATA_CONST") == 0 || strcmp(sc->segname, SEG_DATA) == 0) {
                    findSymbolPtrSection(sc, symbol_sections);
                }
            } else if (lc->cmd == LC_SYMTAB) {
                symbol_table = (const symtab_command*)lc;
            } else if (lc->cmd == LC_DYSYMTAB) {
                dynamic_symbol_table = (const dysymtab_command*)lc;
            }
            lc = (const load_command*)add(lc, lc->cmdsize);
        }

        // can't parse symbol table
        if (text_base == UNDEFINED || link_base == UNDEFINED || symbol_table == NULL) {
            return true;
        }
        loadSymbols(symbol_table, text_base, link_base);

        // can't parse dynamic symbols table
        if (dynamic_symbol_table == NULL || dynamic_symbol_table->nindirectsyms == 0 || symbol_sections.empty()) {
            return true;
        }

        nlist_64* symbols = (nlist_64*)(link_base + symbol_table->symoff);
        uint32_t* dynamic_symbols = (uint32_t*)(link_base + dynamic_symbol_table->indirectsymoff);
        char* string_table = (char*)(link_base + symbol_table->stroff);

        // Load imports for library
        for (auto symbol_section : symbol_sections) {
            loadImports(symbol_section, text_base, symbols, string_table, dynamic_symbols);
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

        CodeCache* cc = new CodeCache(path, count, true);
        UnloadProtection handle(cc);
        if (handle.isValid()) {
            MachOParser parser(cc, image_base);
            if (!parser.parse()) {
                Log::warn("Could not parse symbols from %s", path);
            }
            cc->sort();
            array->add(cc);
        }
    }
}

#endif // __APPLE__
