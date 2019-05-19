#ifndef STRING_H
#define STRING_H

#include <stdbool.h>
#include <stdint.h>

#include "iterator.h"
#include "object.h"

typedef struct string_object {
    // Inherits from Object
    Object  base;

    unsigned length;
    unsigned char_count;
    const char *characters;
} StringObject;

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

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
const StringObject *LoxEmptyString;

typedef struct string_chunk_iterator {
    union {
        Object      object;
        Iterator    iterator;
    };
    StringTreeObject *tree;
    int         pos;
    struct string_chunk_iterator *inner;
} ChunkIterator;

Iterator* LoxStringTree_iterChunks(StringTreeObject*);
#endif
