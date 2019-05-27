#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

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
LoxTable *Hash_newWithSize(size_t size) {
    LoxTable *hashtable = NULL;

    // Check size is a power of 2, and at least 8
    size_t newsize = 8;
    while (newsize < size)
        newsize <<= 1;

    hashtable = object_new(sizeof(LoxTable), &HashType);
    if (unlikely(hashtable == NULL))
        return LoxNIL;

    hashtable->table = calloc(newsize, sizeof(HashEntry));
    if (unlikely(hashtable->table == NULL))
        return LoxNIL;

    hashtable->size = newsize;
    hashtable->size_mask = newsize - 1;

    return hashtable;
}

// My implementation as a table which will auto resize
LoxTable *Hash_new(void) {
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
hash_resize(LoxTable* self, size_t newsize) {
    if (newsize < 1)
        return 1;

    if (newsize < self->count / 2)
        // TODO: Raise error -- it would be resized on new addition
        return 1;

    HashEntry* table = calloc(newsize, sizeof(HashEntry));
    if (unlikely(table == NULL))
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
hash_set_fast(LoxTable *self, Object *key, Object *value, hashval_t hash) {
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
        hash >>= 1;
        slot = hash & self->size_mask;
        entry = self->table + slot;
    }

    if (entry->hash) {
        DECREF(entry->key);
        DECREF(entry->value);
        self->count--;
    }

    *entry = (HashEntry) {
        .value = value,
        .key = key,
        .hash = hash,
    };
    INCREF(key);
    INCREF(value);
    self->count++;
}

static void
hash_set(Object *self, Object *key, Object *value) {
    assert(self->type == &HashType);

    hashval_t hash = 0;
    hash = ht_hashval(key);

    return hash_set_fast((LoxTable*) self, key, value, hash);
}

static HashEntry*
hash_lookup_fast(LoxTable* self, Object* key, hashval_t hash) {
    int slot = hash & self->size_mask;
    HashEntry *entry = self->table + slot;

    /* Step through the table, looking for our value. */
    while (entry->key != NULL
        && (entry->hash != hash
        || !Bool_ISTRUE(entry->key->type->op_eq(entry->key, key))
    )) {
        hash >>= 1;
        slot = hash & self->size_mask;
        entry = self->table + slot;
    }

    /* Did we actually find anything? */
    if (entry->key == NULL) {
        return NULL;
    }
    return entry;
}

static HashEntry*
hash_lookup(LoxTable *self, Object *key) {
    return hash_lookup_fast(self, key, ht_hashval(key));
}

/* Retrieve a key-value pair from a hash table. */
static Object*
hash_get(Object *self, Object *key) {
    assert(self->type == &HashType);
    HashEntry *entry = hash_lookup((LoxTable*) self, key);

    if (entry == NULL)
        // TODO: Raise error
        return LoxUndefined;

    return entry->value;
}

static LoxBool*
hash_contains(Object *self, Object *key) {
    assert(self->type == &HashType);
    HashEntry *entry = hash_lookup((LoxTable*) self, key);
    return (entry == NULL) ? LoxFALSE : LoxTRUE;
}

Object*
Hash_getItem(LoxTable* self, Object* key) {
    assert(self);
    assert(self->base.type == &HashType);

    HashEntry *entry = hash_lookup((LoxTable*) self, key);

    return (entry == NULL) ? NULL : entry->value;
}

Object*
Hash_getItemEx(LoxTable* self, Object* key, hashval_t hash) {
    assert(self);
    HashEntry *rv = hash_lookup_fast(self, key, hash);
    if (rv != NULL)
        return rv->value;

    return NULL;
}

void
Hash_setItem(LoxTable* self, Object* key, Object* value) {
    assert(self);
    hash_set((Object*) self, key, value);
}

void
Hash_setItemEx(LoxTable* self, Object* key, Object* value, hashval_t hash) {
    assert(self);
    hash_set_fast(self, key, value, hash);
}

bool
Hash_contains(LoxTable* self, Object* key) {
    assert(self);
    HashEntry *entry = hash_lookup((LoxTable*) self, key);
    return entry != NULL;
}

static void
hash_remove(Object* self, Object* key) {
    assert(self->type == &HashType);
    HashEntry *entry = hash_lookup((LoxTable*) self, key);

    if (entry == NULL)
        // TODO: Raise error
        return;

    ((LoxTable*) self)->count--;

    DECREF(entry->key);
    DECREF(entry->value);
    entry->key = NULL;
    entry->value = NULL;
}

static Object*
hash_len(Object* self) {
    assert(self != NULL);
    assert(self->type == &HashType);

    return (Object*) Integer_fromLongLong(((LoxTable*) self)->count);
}

static LoxBool*
hash_asbool(Object* self) {
    assert(self != NULL);
    assert(self->type == &HashType);

    return ((LoxTable*) self)->count == 0 ? LoxFALSE : LoxTRUE;
}

static Object*
hash_entries__next(Iterator* this) {
    LoxTableIterator *self = (LoxTableIterator*) this;
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
    return LoxStopIteration;
}

static void
hash_entries__cleanup(Object *self) {
    LoxTableIterator *this = (LoxTableIterator*) self;
    fprintf(stderr, "DECREFING!");
    DECREF(this->hash);
}

Iterator*
Hash_getIterator(LoxTable* self) {
    LoxTableIterator* it = (LoxTableIterator*) LoxIterator_create(sizeof(LoxTableIterator));

    it->iterator.next = hash_entries__next;
    it->iterator.cleanup = hash_entries__cleanup;
    it->hash = self;

    INCREF(self);
    return (Iterator*) it;
}

static Iterator*
hash_iterate(Object *self) {
    assert(self->type == &HashType);
    return Hash_getIterator((LoxTable*) self);
}

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

static Object*
hash_asstring(Object* self) {
    assert(self->type == &HashType);

    char buffer[1024];  // TODO: Use the + operator of LoxString
    char* position = buffer;
    int remaining = sizeof(buffer) - 1, bytes;

    bytes = snprintf(position, remaining, "{");
    position += bytes, remaining -= bytes;

    Object *next, *key, *value;
    LoxString *skey, *svalue;

    Iterator* it = Hash_getIterator((LoxTable*) self);
    while (LoxStopIteration != (next = it->next(it))) {
        assert(Tuple_isTuple(next));
        key = Tuple_getItem((LoxTuple*) next, 0);
        value = Tuple_getItem((LoxTuple*) next, 1);
        skey = (LoxString*) key->type->as_string(key);
        svalue = (LoxString*) value->type->as_string(value);
        bytes = snprintf(position, remaining, "%.*s: %.*s, ",
            skey->length, skey->characters, svalue->length, svalue->characters);
        position += bytes, remaining -= bytes;
    }
    position += snprintf(max((char*) buffer + 1, position - 2), remaining, "}");

    return (Object*) String_fromCharArrayAndSize(buffer, position - buffer);
}

static void
hash_cleanup(Object *self) {
    assert(self->type == &HashType);

    LoxTable *this = (LoxTable*) self;
    HashEntry* table = this->table;
    int p = this->size;
    while (p--) {
        if (table[p].key != NULL) {
            DECREF(table[p].key);
            DECREF(table[p].value);
        }
    }

    free(this->table);
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
    .cleanup = hash_cleanup,
};
