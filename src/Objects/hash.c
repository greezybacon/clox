#define _XOPEN_SOURCE 500 /* Enable certain library functions (strdup) on linux.  See feature_test_macros(7) */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "hash.h"
#include "boolean.h"
#include "integer.h"
#include "object.h"
#include "garbage.h"
#include "string.h"
#include "tuple.h"

static struct object_type HashType;

// A quick hashtable implementation based on
// https://gist.github.com/tonious/1377667

/* Create a new hashtable. */
HashObject *Hash_newWithSize(size_t size) {
	HashObject *hashtable = NULL;
    // XXX: Check size is a power of 2

	if ( size < 1 )
        return NULL;

    hashtable = object_new(sizeof(HashObject), &HashType);
    if (hashtable == NULL)
        return hashtable;
    
    hashtable->table = calloc(size, sizeof(HashEntry));
    if (hashtable->table == NULL) {
        free(hashtable);
        return NULL;
    }
    hashtable->size = size;

	return hashtable;
}

// My implementation as a table which will auto resize
HashObject *Hash_new(void) {
    return Hash_newWithSize(8);
}

/* Hash a string for a particular hash table. */
static int
ht_hashslot(int size, Object *key) {
    assert(key->type->hash);
	return key->type->hash(key) % (size - 1);
}

static int
hash_resize(HashObject* self, size_t newsize) {
    if (newsize < 1)
        return 1;

    if (newsize < self->count / 2)
        // TODO: Raise error -- it would be resized on new addition
        return 1;

    HashEntry* table = calloc(newsize, sizeof(HashEntry));
    if (table == NULL)
        return errno;

    // Place all the keys in the new table
    int slot, i;
    HashEntry *entry, *current;
    for (current = self->table, i=self->size; i; current++, i--) {
        if (current->key != NULL) {
        	slot = ht_hashslot(newsize, current->key);
            assert(slot < newsize);

            // Find an empty slot
        	entry = table + slot;
        	while (entry->key != NULL) {
                slot = (slot + 1) % newsize;
                entry = table + slot;
        	}

            entry->value = current->value;
            entry->key = current->key;
        }
    }
    free(self->table);
    self->table = table;
    self->size = newsize;

    return 0;
}

/* Insert a key-value pair into a hash table. */
static void
hash_set(HashObject *self, Object *key, Object *value) {
    assert(((Object*)self)->type == &HashType);

	int slot = 0;
    HashEntry *entry;

    // If the table is over half full, double the size
    if (self->count >= self->size / 2) {
        if (0 != hash_resize(self, self->size * 2))
            // TODO: Raise error?
            ;
    }
    // TODO: In case resize failed, check that there are at least two slots
    // in the table. If there are not, then this insert should fail. There
    // must be at least one empty slot or lookups for missing items would
    // loop forever.

	slot = ht_hashslot(self->size, key);
    assert(slot < self->size);

	entry = self->table + slot;
	while (entry->key != NULL) {
        if (LoxTRUE == entry->key->type->op_eq(entry->key, key)) {
	        // There's something associated with this key. Let's replace it
            DECREF(entry->value);
            INCREF(value);
    		entry->value = value;
            return;
        }
        slot = (slot + 1) % self->size;
        entry = self->table + slot;
	}

    INCREF(value);
    INCREF(key);
    entry->value = value;
    entry->key = key;
    self->count++;
}

static HashEntry*
hash_lookup_fast(HashObject* self, Object* key, int slot) {
	HashEntry *entry;

    assert(slot < self->size);

    entry = self->table + slot;

	/* Step through the table, looking for our value. */
	while (entry->key != NULL
        && LoxFALSE == entry->key->type->op_eq(entry->key, key)
    ) {
        slot = (slot + 1) % self->size;
        entry = self->table + slot;
	}

	/* Did we actually find anything? */
	if (entry->key == NULL) {
        return NULL;
    }
	return entry;
}

static HashEntry*
hash_lookup(HashObject *self, Object *key) {
    return hash_lookup_fast(self, key, ht_hashslot(self->size, key));
}

/* Retrieve a key-value pair from a hash table. */
static Object*
hash_get(Object *self, Object *key) {
    assert(self->type == &HashType);
	HashEntry *entry = hash_lookup((HashObject*) self, key);

    if (entry == NULL)
        // TODO: Raise error
        return NULL;

    return entry->value;
}

static void
hash_remove(Object* self, Object* key) {
    assert(self->type == &HashType);
	HashEntry *entry = hash_lookup((HashObject*) self, key);

    if (entry == NULL)
        // TODO: Raise error
        ;

    DECREF(entry->value);
    DECREF(entry->key);
    ((HashObject*) self)->count--;
    
    entry->key = NULL;
    entry->value = NULL;
}

static Object*
hash_len(Object* self) {
    assert(self != NULL);
    assert(self->type == &HashType);

    return (Object*) Integer_fromLongLong(((HashObject*) self)->count);
}

static Object*
hash_asbool(Object* self) {
    assert(self != NULL);
    assert(self->type == &HashType);

    return ((HashObject*) self)->count == 0 ? LoxFALSE : LoxTRUE;
}

typedef struct _iterator {
    Object* (*next)(struct _iterator*);
} Iterator;

typedef struct {
    Iterator    base;
    int         pos;
    HashObject* hash;
} HashObjectIterator;

static Object*
hash_entries__next(Iterator* this) {
    HashObjectIterator *self = (HashObjectIterator*) this;
    int p;
    HashEntry* table = self->hash->table;
    while (self->pos < self->hash->size) {
        p = self->pos++;
        if (table[p].key != NULL) {
            return Tuple_fromArgs(2, table[p].key, table[p].value);
        }
    }
    // Send terminating sentinel
    return NULL;
}

static Iterator*
iter_hash_entries(HashObject* self) {
    HashObjectIterator* it = malloc(sizeof(HashObjectIterator));
    
    *it = (HashObjectIterator) {
        .base.next = hash_entries__next,
        .hash = self,
        .pos = 0,
    };
    return (Iterator*) it;
}

static Object*
hash_asstring(Object* self) {
    assert(self->type == &HashType);

    char buffer[1024];  // TODO: Use the + operator of StringObject
    char* position = buffer;
    int remaining = sizeof(buffer) - 1, bytes;

    bytes = snprintf(position, remaining, "{");
    position += bytes, remaining -= bytes;

    Object *next, *key, *value;
    StringObject *skey, *svalue;

    HashObjectIterator* it = (HashObjectIterator*) iter_hash_entries((HashObject*) self);
    while ((next = it->base.next((Iterator*) it))) {
        key = (Object*) Tuple_getItem(next, 0);
        value = (Object*) Tuple_getItem(next, 1);
        skey = (StringObject*) key->type->as_string(key);
        svalue = (StringObject*) value->type->as_string(value);
        bytes = snprintf(position, remaining, "%.*s: %.*s, ",
            skey->length, skey->characters, svalue->length, svalue->characters);
        position += bytes, remaining -= bytes;
        DECREF((Object*) skey);
        DECREF((Object*) svalue);
        DECREF(next);
    }
    position += snprintf(position, remaining, "}");

    free(it);

    return (Object*) String_fromCharArrayAndSize(buffer, position - buffer);
}

static void
hash_free(Object* self) {
    assert(self != NULL);
    assert(self->type == &HashType);

    int i;
    HashObject *hash = (HashObject*) self;
    HashEntry *entry;
    for (entry = hash->table, i=hash->size; i; entry++, i--) {
        if (entry->key != NULL) {
            DECREF(entry->value);
            DECREF(entry->key);
        }
    }
}

static struct object_type HashType = (ObjectType) {
    .code = TYPE_HASH,
    .name = "hash",

    .len = hash_len,
    .set_item = hash_set,
    .get_item = hash_get,
    .remove_item = hash_remove,
    
    .as_string = hash_asstring,
    .as_bool = hash_asbool,

    .cleanup = hash_free,
};
