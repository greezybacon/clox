#ifndef STRING_H
#define STRING_H

#include "object.h"

typedef struct string_object {
    // Inherits from Object
    Object  base;

    unsigned length;
    unsigned char_count;
    char    *characters;
} StringObject;

StringObject* String_fromCharArrayAndSize(char*, size_t);

#endif
