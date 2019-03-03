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
#include "iterator.h"
#include "object.h"
#include "string.h"
#include "tuple.h"

#include "Vendor/bdwgc/include/gc.h"

static struct object_type HashType;

// A quick hashtable implementation based on
// https://gist.github.com/tonious/1377667

/* Create a new hashtable. */
HashObject *Hash_newWithSize(size_t size) {
    HashObject *hashtable = NULL;

    // Check size is a power of 2, and at least 8
    size_t newsize = 8;
    while (newsize < size)
        newsize <<= 1;

    hashtable = object_new(sizeof(HashObject), &HashType);
    if (hashtable == NULL)
        return hashtable;

    hashtable->table = GC_MALLOC(newsize * sizeof(HashEntry));
    if (hashtable->table == NULL)
        return NULL;

    hashtable->size = newsize;
    hashtable->size_mask = newsize - 1;

    return hashtable;
}

// My implementation as a table which will auto resize
HashObject *Hash_new(void) {
    return Hash_newWithSize(8);
}

/* Hash a string for a particular hash table. */
static hashval_t
ht_hashval(Object *key) {
    if (key->type->hash)
        return key->type->hash(key);
    else
        return (hashval_t) (void*) key;
}

static int
hash_resize(HashObject* self, size_t newsize) {
    if (newsize < 1)
        return 1;

    if (newsize < self->count / 2)
        // TODO: Raise error -- it would be resized on new addition
        return 1;

    HashEntry* table = GC_MALLOC(newsize * sizeof(HashEntry));
    if (table == NULL)
        return errno;

    // Place all the keys in the new table
    int slot, i;
    size_t mask = newsize - 1;
    HashEntry *entry, *current;
    for (current = self->table, i=self->size; i; current++, i--) {
        if (current->key != NULL) {
            slot = current->hash & mask;

            // Find an empty slot
            entry = table + slot;
            while (entry->key != NULL) {
                slot = (slot + 1) & mask;
                entry = table + slot;
            }

            *entry = *current;
        }
    }
    self->table = table;
    self->size = newsize;
    self->size_mask = mask;

    return 0;
}

static Object* hash_asstring(Object*);

/* Insert a key-value pair into a hash table. */
static void
hash_set_fast(HashObject *self, Object *key, Object *value, hashval_t hash) {
    int slot;
    HashEntry *entry;

    // If the table is over half full, double the size
    if (self->count >= self->size - 2) {
        if (0 != hash_resize(self, self->size * 2))
            // TODO: Raise error?
            ;
    }
    // TODO: In case resize failed, check that there are at least two slots
    // in the table. If there are not, then this insert should fail. There
    // must be at least one empty slot or lookups for missing items would
    // loop forever.

    slot = hash & self->size_mask;

    entry = self->table + slot;
    while (entry->key != NULL) {
        if (LoxTRUE == entry->key->type->op_eq(entry->key, key)) {
            // There's something associated with this key. Let's replace it
            entry->value = value;
            return;
        }
        slot = (slot + 1) & self->size_mask;
        entry = self->table + slot;
    }

    *entry = (HashEntry) {
        .value = value,
        .key = key,
        .hash = hash,
    };
    self->count++;
}

static void
hash_set(Object *self, Object *key, Object *value) {
    assert(self->type == &HashType);

    hashval_t hash = 0;
    hash = ht_hashval(key);

    return hash_set_fast((HashObject*) self, key, value, hash);
}

static HashEntry*
hash_lookup_fast(HashObject* self, Object* key, hashval_t hash) {
    int slot = hash & self->size_mask;
    HashEntry *entry = self->table + slot;

    /* Step through the table, looking for our value. */
    while (entry->key != NULL
        && (entry->hash != hash
        || !Bool_ISTRUE(entry->key->type->op_eq(entry->key, key))
    )) {
        slot = (slot + 1) & self->size_mask;
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
    return hash_lookup_fast(self, key, ht_hashval(key));
}

/* Retrieve a key-value pair from a hash table. */
static Object*
hash_get(Object *self, Object *key) {
    assert(self->type == &HashType);
    HashEntry *entry = hash_lookup((HashObject*) self, key);

    if (entry == NULL)
        // TODO: Raise error
        return LoxNIL;

    return entry->value;
}

static BoolObject*
hash_contains(Object *self, Object *key) {
    assert(self->type == &HashType);
    HashEntry *entry = hash_lookup((HashObject*) self, key);
    return (entry == NULL) ? LoxFALSE : LoxTRUE;
}

Object*
Hash_getItem(HashObject* self, Object* key) {
    assert(self);
    assert(self->base.type == &HashType);

    HashEntry *entry = hash_lookup((HashObject*) self, key);

    return (entry == NULL) ? NULL : entry->value;
}

Object*
Hash_getItemEx(HashObject* self, Object* key, hashval_t hash) {
    assert(self);
    HashEntry *rv = hash_lookup_fast(self, key, hash);
    if (rv != NULL)
        return rv->value;

    return NULL;
}

void
Hash_setItem(HashObject* self, Object* key, Object* value) {
    assert(self);
    hash_set((Object*) self, key, value);
}

void
Hash_setItemEx(HashObject* self, Object* key, Object* value, hashval_t hash) {
    assert(self);
    hash_set_fast(self, key, value, hash);
}

bool
Hash_contains(HashObject* self, Object* key) {
    assert(self);
    HashEntry *entry = hash_lookup((HashObject*) self, key);
    return entry != NULL;
}

static void
hash_remove(Object* self, Object* key) {
    assert(self->type == &HashType);
    HashEntry *entry = hash_lookup((HashObject*) self, key);

    if (entry == NULL)
        // TODO: Raise error
        ;

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

static BoolObject*
hash_asbool(Object* self) {
    assert(self != NULL);
    assert(self->type == &HashType);

    return ((HashObject*) self)->count == 0 ? LoxFALSE : LoxTRUE;
}

static Object*
hash_entries__next(Iterator* this) {
    HashObjectIterator *self = (HashObjectIterator*) this;
    int p;
    HashEntry* table = self->hash->table;
    while (self->pos < self->hash->size) {
        p = self->pos++;
        if (table[p].key != NULL) {
            return this->previous
                = (Object*) Tuple_fromArgs(2, table[p].key, table[p].value);
        }
    }
    // Send terminating sentinel
    return NULL;
}

Iterator*
Hash_getIterator(HashObject* self) {
    HashObjectIterator* it = GC_NEW(HashObjectIterator);

    *it = (HashObjectIterator) {
        .base.next = hash_entries__next,
        .hash = self,
        .pos = 0,
    };
    return (Iterator*) it;
}

static Iterator*
hash_iterate(Object *self) {
    assert(self->type == &HashType);
    return Hash_getIterator((HashObject*) self);
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

    Iterator* it = Hash_getIterator((HashObject*) self);
    while ((next = it->next(it))) {
        assert(Tuple_isTuple(next));
        key = Tuple_getItem((TupleObject*) next, 0);
        value = Tuple_getItem((TupleObject*) next, 1);
        skey = (StringObject*) key->type->as_string(key);
        svalue = (StringObject*) value->type->as_string(value);
        bytes = snprintf(position, remaining, "%.*s: %.*s, ",
            skey->length, skey->characters, svalue->length, svalue->characters);
        position += bytes, remaining -= bytes;
    }
    position += snprintf(position, remaining, "}");

    free(it);

    return (Object*) String_fromCharArrayAndSize(buffer, position - buffer);
}

static struct object_type HashType = (ObjectType) {
    .code = TYPE_HASH,
    .name = "hash",

    .len = hash_len,
    .set_item = hash_set,
    .get_item = hash_get,
    .remove_item = hash_remove,
    .contains = hash_contains,
    .iterate = hash_iterate,

    .as_string = hash_asstring,
    .as_bool = hash_asbool,
};
