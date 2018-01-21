#ifndef HASH_H
#define HASH_H

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
} HashEntry;

typedef struct hash_object {
    // Inherits from Object
    Object  base;

    size_t  count;
    size_t  size;
    HashEntry *table;
} HashObject;

HashObject* Hash_new(void);
HashObject* Hash_newWithSize(size_t);

#endif

