#ifndef STRING_H
#define STRING_H

#include <stdbool.h>
#include <stdint.h>

#include "iterator.h"
#include "object.h"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

typedef struct string_object {
    // Inherits from Object
    Object  base;

    unsigned length;
    unsigned char_count;
    const char *characters;
} LoxString;

typedef struct {
    union {
        Object      object;
        Iterator    iterator;
    };
    int         pos;
} LoxStringIterator;

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

LoxString* String_fromCharsAndSize(const char*, size_t);
bool String_isString(Object*);
LoxString* String_fromObject(Object*);
LoxString* String_fromLiteral(const char*, size_t);
LoxString* String_fromConstant(const char *);
LoxString* String_fromMalloc(const char *, size_t);
size_t String_getLength(Object* self);
int String_compare(LoxString*, const char*);
LoxString* String_fromConstant(const char*);
Object* LoxString_Build(int, ...);
Object* LoxString_BuildFromList(int, Object **);

const LoxString *LoxEmptyString;

typedef struct stringtree_object {
    // Inherits from Object
    Object  base;

    Object  *left;
    Object  *right;
} LoxStringTree;

LoxStringTree* StringTree_fromStrings(Object*, Object*);
bool StringTree_isStringTree(Object *);

typedef struct string_chunk_iterator {
    union {
        Object      object;
        Iterator    iterator;
    };
    int             pos;
    Iterator        *inner;
} ChunkIterator;

Iterator* LoxStringTree_iterChunks(LoxStringTree*);
#endif
