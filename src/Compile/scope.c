#include <assert.h>
#include <stdio.h>

#include "vm.h"
#include "Objects/boolean.h"
#include "Vendor/bdwgc/include/gc.h"
#include "Objects/hash.h"

VmScope*
VmScope_create(VmScope *outer, CodeContext *code, Object **locals, unsigned locals_count) {
    VmScope *self = GC_MALLOC(sizeof(VmScope));
    *self = (VmScope) {
        .outer = outer,
        .globals = outer->globals,
        .code = code,
    };
    if (locals_count) {
        self->locals_count = locals_count,
        self->locals = GC_MALLOC(sizeof(Object*) * locals_count);
        while (locals_count--)
            *(self->locals + locals_count) = *(locals + locals_count);
    }
    return self;
}

VmScope*
VmScope_leave(VmScope* self) {
    VmScope* rv = self->outer;
    return rv;
}

#include "Objects/string.h"

Object*
VmScope_lookup_global(VmScope* self, Object* name, hashval_t hash) {
    // Search globals
    assert(self);

    Object *rv;
    if (self->globals && (rv = Hash_getItemEx(self->globals, name, hash)))
        return rv;

    if (self->outer)
        return VmScope_lookup_global(self->outer, name, hash);

    return LoxNIL;

}

Object*
VmScope_lookup_local(VmScope *self, unsigned index) {
    assert(self);

    if (index >= self->locals_count)
        return LoxNIL;

    return *(self->locals + index);
}

void
VmScope_assign(VmScope* self, Object* name, Object* value, hashval_t hash) {
    // TODO: Assign local?
    if (self->globals)
        return Hash_setItemEx(self->globals, name, value, hash);

    // Trigger fatal error
    printf("Unable to find variable\n");
}
