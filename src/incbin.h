#ifndef _INCBIN_H
#define _INCBIN_H

#define INCBIN(NAME, FILE) \
    extern const char NAME[];\
    extern const char NAME##_END[];\
    asm(\
     ".section \".rodata\", \"a\", @progbits\n"\
     #NAME ":\n"\
     ".incbin \"" FILE "\"\n"\
     #NAME "_END:\n"\
      ".previous\n"\
     ".byte 0x00"\
    );

#define INCBIN_SIZEOF(NAME) (NAME##_END - NAME)

#endif // _INCBIN_H