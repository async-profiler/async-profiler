/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __APPLE__

#include <set>
#include <dlfcn.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include "symbols.h"
#include "log.h"


class MachOParser {
  private:
    CodeCache* _cc;
    const mach_header* _image_base;

    static const char* add(const void* base, uint64_t offset) {
        return (const char*)base + offset;
    }

    const section_64* findSymbolPtrSection(const segment_command_64* sc) {
        const section_64* section = (const section_64*)add(sc, sizeof(segment_command_64));
        for (uint32_t i = 0; i < sc->nsects; i++) {
            if (strcmp(section->sectname, "__la_symbol_ptr") == 0) {
                return section;
            }
            section++;
        }
        return NULL;
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

    void loadImports(const symtab_command* symtab, const dysymtab_command* dysymtab,
                     const section_64* la_symbol_ptr, const char* link_base) {
        const nlist_64* sym = (const nlist_64*)add(link_base, symtab->symoff);
        const char* str_table = add(link_base, symtab->stroff);

        const uint32_t* isym = (const uint32_t*)add(link_base, dysymtab->indirectsymoff) + la_symbol_ptr->reserved1;
        uint32_t isym_count = la_symbol_ptr->size / sizeof(void*);
        void** slot = (void**)add(_image_base, la_symbol_ptr->addr);

        for (uint32_t i = 0; i < isym_count; i++) {
            const char* name = str_table + sym[isym[i]].n_un.n_strx;
            if (name[0] == '_') name++;
            _cc->addImport(&slot[i], name);
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
        const section_64* la_symbol_ptr = NULL;
        const symtab_command* symtab = NULL;

        for (uint32_t i = 0; i < header->ncmds; i++) {
            if (lc->cmd == LC_SEGMENT_64) {
                const segment_command_64* sc = (const segment_command_64*)lc;
                if ((sc->initprot & 4) != 0) {
                    if (text_base == UNDEFINED || strcmp(sc->segname, "__TEXT") == 0) {
                        text_base = (const char*)_image_base - sc->vmaddr;
                        _cc->setTextBase(text_base);
                        _cc->updateBounds(_image_base, add(_image_base, sc->vmsize));
                    }
                } else if ((sc->initprot & 7) == 1) {
                    if (link_base == UNDEFINED || strcmp(sc->segname, "__LINKEDIT") == 0) {
                        link_base = text_base + sc->vmaddr - sc->fileoff;
                    }
                } else if ((sc->initprot & 2) != 0) {
                    if (strcmp(sc->segname, "__DATA") == 0) {
                        la_symbol_ptr = findSymbolPtrSection(sc);
                    }
                }
            } else if (lc->cmd == LC_SYMTAB) {
                symtab = (const symtab_command*)lc;
                if (text_base != UNDEFINED && link_base != UNDEFINED) {
                    loadSymbols(symtab, text_base, link_base);
                }
            } else if (lc->cmd == LC_DYSYMTAB) {
                if (la_symbol_ptr != NULL && symtab != NULL && link_base != UNDEFINED) {
                    loadImports(symtab, (const dysymtab_command*)lc, la_symbol_ptr, link_base);
                }
            }
            lc = (const load_command*)add(lc, lc->cmdsize);
        }

        return true;
    }
};


Mutex Symbols::_parse_lock;
bool Symbols::_have_kernel_symbols = false;
static std::set<const void*> _parsed_libraries;

void Symbols::parseKernelSymbols(CodeCache* cc) {
}

void Symbols::parseLibraries(CodeCacheArray* array, bool kernel_symbols) {
    MutexLocker ml(_parse_lock);
    uint32_t images = _dyld_image_count();

    for (uint32_t i = 0; i < images; i++) {
        const mach_header* image_base = _dyld_get_image_header(i);
        if (image_base == NULL || !_parsed_libraries.insert(image_base).second) {
            continue;  // the library was already parsed
        }

        int count = array->count();
        if (count >= MAX_NATIVE_LIBS) {
            break;
        }

        const char* path = _dyld_get_image_name(i);

        // Protect the library from unloading while parsing symbols
        void* handle = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
        if (handle == NULL) {
            continue;
        }

        CodeCache* cc = new CodeCache(path, count, true);
        MachOParser parser(cc, image_base);
        if (!parser.parse()) {
            Log::warn("Could not parse symbols from %s", path);
        }
        dlclose(handle);

        cc->sort();
        array->add(cc);
    }
}

#endif // __APPLE__
