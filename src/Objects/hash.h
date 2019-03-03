#ifndef HASH_H
#define HASH_H

#include <stdbool.h>

#include "iterator.h"
#include "object.h"

#if 0
struct _entry_s {
	Object *key;
	Object *value;
	struct entry_s *next;
};
typedef struct entry_s _HashEntry;

typedef struct _hash_object {
    // Inherits from Object
    Object  base;

    size_t  size;
    size_t  count;
    HashEntry **table;
} _HashObject;

#endif

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

