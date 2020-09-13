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
    if (unlikely(self->protect_delete))
        return;

    if (self->type->cleanup != NULL)
        self->type->cleanup(self);

    if (unlikely(self->type->features & FEATURE_STASH))
        if (object_trystash(self))
            return;

    free(self);
}

static LoxTable*
object_setup_props(Object *self) {
    LoxTable *methods;
    unsigned count;
    ObjectProperty* method = self->type->properties;
    Object *value;

    while (method && method->name) {
        count++;
        method++;
    }

    self->type->_methodTable = methods = Hash_newWithSize(count);
    INCREF((Object*) methods);
    method = self->type->properties;
    while (method && method->name) {
        if (method->type == OBJECT_PROP_TYPE_METHOD) {
            value = NativeFunction_new(method->method);
        }
        else if (method->type == OBJECT_PROP_TYPE_PROPERTY) {
            value = LoxNativeProperty_create(method->property.getter,
                method->property.setter, NULL);
        }
        else {
            value = method->value;
        }
        Hash_setItem(methods, (Object*) String_fromConstant(method->name),
            value);
        method++;
    }

    return methods;
}

Object*
object_getattr(Object* self, Object *name, hashval_t hash) {
    assert(self);

    if (self->type->getattr)
        return self->type->getattr(self, name, hash);

    // Build a HashTable for faster access to methods
    if (self->type->properties) {
        LoxTable *methods = self->type->_methodTable;
        if (!methods)
            methods = object_setup_props(self);

        assert(String_isString(name));

        Object *result;
        if (NULL != (result = Hash_getItemEx(methods, name, hash))) {
            if (LoxNativeProperty_isProperty(result))
                result = ((LoxNativeProperty*) result)->getter;
            if (Function_isNativeFunction(result))
                result = NativeFunction_bind(result, self);
        }

        return result ? result : LoxUndefined;
    }

    return LoxUndefined;
}