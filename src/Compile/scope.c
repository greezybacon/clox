#include <assert.h>
#include <stdio.h>

#include "vm.h"
#include "Objects/boolean.h"
#include "Vendor/bdwgc/include/gc.h"
#include "Objects/hash.h"

static void
VmScope_cleanup(GC_PTR object, GC_PTR client_data) {
    VmScope* self = (VmScope*) object;

    int i = self->locals_count;
    while (i--) {
        DECREF(*(self->locals + i));
    }
}

VmScope*
VmScope_create(VmScope *outer, CodeContext *code, Object **locals, unsigned locals_count) {
    VmScope *self = GC_MALLOC(sizeof(VmScope));
    *self = (VmScope) {
        .outer = outer,
        .globals = outer->globals,
        .code = code,
        .locals_count = locals_count,
        .locals = locals,
    };
    GC_REGISTER_FINALIZER(self, VmScope_cleanup, NULL, NULL, NULL);
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

    return LoxUndefined;

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
