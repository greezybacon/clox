#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "object.h"
#include "function.h"
#include "string.h"
#include "Vendor/bdwgc/include/gc.h"

static void
object_finalize(GC_PTR object, GC_PTR client_data) {
    Object* self = (Object*) object;
    assert(self->type != NULL);
    assert(self->type->cleanup != NULL);

    self->type->cleanup(self);
}

typedef struct stash_heap {
    Object             *target;
    struct stash_heap *next;
} StashHeapEntry;

static StashHeapEntry* StashHead = NULL;
static StashHeapEntry* StashAvailable = NULL;
static StashHeapEntry stash[32] = { { 0 } };

void*
object_new(size_t size, ObjectType* type) {
    if (unlikely(type->features & FEATURE_STASH)) {
        StashHeapEntry *entry = StashHead, *prev = NULL;
        while (entry) {
            if (entry->target->type == type) {
                if (prev)
                    prev->next = entry->next;
                else if (StashHead == entry)
                    StashHead = entry->next;
                entry->next = StashAvailable;
                StashAvailable = entry;
                return entry->target;
            }
            prev = entry;
            entry = entry->next;
        }
    }

    Object* result = (void*) calloc(1, size);

    if (unlikely(result == NULL)) {
        // TODO: Trigger error
    }

    *result = (Object) {
        .type = type,
        .refcount = 0,
    };

    return (void*) result;
}

static bool
object_trystash(Object* self) {
    static bool initialized = false;
    StashHeapEntry *entry;

    if (unlikely(!initialized)) {
        int i;
        entry = &stash[0];
        for (i = 0; i < 32; i++) {
            entry->next = StashAvailable;
            StashAvailable = entry;
            entry++;
        }
        initialized = true;
    }

    entry = StashAvailable;
    if (entry) {
        StashAvailable = entry->next;
        *entry = (StashHeapEntry) {
            .next = StashHead,
            .target = self,
        };
        StashHead = entry;
        return true;
    }
   
    // otherwise drop oldest entry?
    return false;
}

void
LoxObject_Cleanup(Object* self) {
    if (unlikely(self->type->cleanup != NULL))
        self->type->cleanup(self);

    if (unlikely(self->type->features & FEATURE_STASH))
        if (object_trystash(self))
            return;

    free(self);
}

Object*
object_getattr(Object* self, Object *name) {
    assert(self);

    if (self->type->getattr)
        return self->type->getattr(self, name);

    if (self->type->methods) {
        // XXX: This is terribly slow -- move to a automatically-created hashtable?
        ObjectMethod* method = self->type->methods;
        while (method) {
            if (String_compare(name, method->name) == 0)
                return NativeFunction_bind(NativeFunction_new(method->method), self);
            method++;
        }
    }

    return LoxNIL;
}