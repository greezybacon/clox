#include <assert.h>
#include <stdio.h>

#include "vm.h"
#include "Objects/boolean.h"
#include "Objects/garbage.h"
#include "Objects/hash.h"

VmScope*
VmScope_create(VmScope *outer, Object **locals, CodeContext *code) {
    VmScope *scope = malloc(sizeof(VmScope));
    *scope = (VmScope) {
        .outer = outer,
        .code = code,
        .locals = locals,
    };
    return scope;
}

VmScope*
VmScope_leave(VmScope* self) {
    VmScope* rv = self->outer;
    free(self);
    return rv;
}

#include "Objects/string.h"
#include "Objects/garbage.h"

static Object*
VmScope_lookup_local(VmScope* self, Object* name) {
    int index = 0;
    if (!self->code || !self->locals)
        return NULL;

    LocalsList *locals = &self->code->locals;

    // Direct search through the locals list
    while (index < locals->count) {
        if (LoxTRUE == name->type->op_eq(name, *(locals->names + index))) {
            // Found it in the locals list
            return *(self->locals + index);
        }
        index++;
    }

    // Search outer scope(s)
    if (self->outer)
        return VmScope_lookup_local(self->outer, name);

    return NULL;
}

Object*
VmScope_lookup(VmScope* self, Object* name) {
    Object *rv;
    if (self->locals && (rv = VmScope_lookup_local(self, name)))
        return rv;

    // Search globals
    if (self->globals && (rv = Hash_getItem(self->globals, name)))
        return rv;

    return LoxNIL;
}

static bool
VmScope_assign_local(VmScope* self, Object* name, Object* value) {
    // See if the variable is in the immediately outer scope (stack)
    int index = 0;
    LocalsList *locals = &self->code->locals;
    Object *old;

    // Direct search through the locals list
    while (index < locals->count) {
        if (LoxTRUE == name->type->op_eq(name, *(locals->names + index))) {
            // Found it in the locals list
            Object *old = *(self->locals + index);
            *(self->locals + index) = value;
            if (old != NULL) {
                fprintf(stderr, "DECREF %p\n", old);
                DECREF(old);
            }
            return true;
        }
        index++;
    }
    
    // ???: Unable to find in this scope -- look further out
    if (self->outer)
        return VmScope_assign_local(self->outer, name, value);

    return false;
}

void
VmScope_assign(VmScope* self, Object* name, Object* value) {
    Object *rv;
    if (self->locals && VmScope_assign_local(self, name, value))
        return;

    // TODO: Assign global?
    if (self->globals)
        return Hash_setItem(self->globals, name, value);

    // Trigger fatal error
    printf("Unable to find variable\n");
}