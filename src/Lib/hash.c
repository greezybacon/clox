#define _XOPEN_SOURCE 500 /* Enable certain library functions (strdup) on linux.  See feature_test_macros(7) */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "hash.h"
#include "object.h"

static struct object_type HashType;

// A quick hashtable implementation based on
// https://gist.github.com/tonious/1377667

/* Create a new hashtable. */
HashObject *Hash_newWithSize(size_t size) {
	HashObject *hashtable = NULL;
	int i;
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

	*hashtable = (HashObject) {
        .size = size,
        .count = 0,
    };

	return hashtable;	
}

// My implementation as a table which will auto resize
HashObject *Hash_new(void) {
    return Hash_newWithSize(8);
}

/* Hash a string for a particular hash table. */
static int 
ht_hashslot(int size, Object *key) {
    if (key->type->hash == NULL)
        // Trigger error
        ;

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
        if (entry->key->type->compare(entry->key, key) == 0) {
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
hash_lookup(HashObject *self, Object *key) {
	int slot = 0;
	HashEntry *entry;

	slot = ht_hashslot(self->size, key);
    assert(slot < self->size);

    entry = self->table + slot;
    
	/* Step through the table, looking for our value. */
	while (entry->key != NULL 
        && entry->key->type->compare(entry->key, key) != 0
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

/* Retrieve a key-value pair from a hash table. */
static Object*
hash_get(HashObject *self, Object *key) {
	HashEntry *entry = hash_lookup(self, key);
    
    if (entry == NULL)
        // TODO: Raise error
        ;

    return entry->value;
}

static void
hash_remove(HashObject* self, Object* key) {
	HashEntry *entry = hash_lookup(self, key);

    if (entry == NULL)
        // TODO: Raise error
        ;

    DECREF(entry->value);
    DECREF(entry->key);
    self->count--;
    
    entry->key = NULL;
    entry->value = NULL;
}

static Object*
hash_len(Object* self) {
    assert(self != NULL);
    assert(self->type == &HashType);

    return Integer_fromLongLong(((HashObject*) self)->count);
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

    .cleanup = hash_free,
};
