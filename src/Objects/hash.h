#ifndef HASH_H
#define HASH_H

#include <stdbool.h>

#include "iterator.h"
#include "object.h"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

typedef struct entry_s {
    Object *key;
    Object *value;
    hashval_t hash;
} HashEntry;

typedef struct hash_object {
    // Inherits from Object
    Object  base;

    size_t  count;
    size_t  size;
    size_t  size_mask;
    HashEntry *table;
} HashObject;

typedef struct {
    Iterator    base;
    int         pos;
    HashObject* hash;
} HashObjectIterator;

HashObject* Hash_new(void);
HashObject* Hash_newWithSize(size_t);
Object* Hash_getItem(HashObject*, Object*);
Object* Hash_getItemEx(HashObject*, Object*, hashval_t);
bool Hash_contains(HashObject*, Object*);
void Hash_setItem(HashObject*, Object*, Object*);
void Hash_setItemEx(HashObject*, Object*, Object*, hashval_t);
Iterator* Hash_getIterator(HashObject*);

#endif

