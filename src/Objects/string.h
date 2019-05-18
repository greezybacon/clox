#ifndef STRING_H
#define STRING_H

#include <stdbool.h>

#include "object.h"

typedef struct string_object {
    // Inherits from Object
    Object  base;

    unsigned length;
    unsigned char_count;
    const char *characters;
} StringObject;

StringObject* String_fromCharArrayAndSize(char*, size_t);
bool String_isString(Object*);
StringObject* String_fromObject(Object*);
StringObject* String_fromLiteral(char*, size_t);
StringObject* String_fromConstant(const char *);
StringObject* String_fromMalloc(const char *, size_t);
size_t String_getLength(Object* self);
int String_compare(StringObject*, const char*);
StringObject* String_fromConstant(const char*);

typedef struct stringtree_object {
    // Inherits from Object
    Object  base;

    Object  *left;
    Object  *right;
} StringTreeObject;

StringTreeObject* StringTree_fromStrings(Object*, Object*);

#endif
