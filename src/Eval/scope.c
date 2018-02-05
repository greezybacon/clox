#include <assert.h>
#include <stdio.h>

#include "scope.h"
#include "Objects/boolean.h"
#include "Objects/garbage.h"
#include "Objects/hash.h"

Scope*
Scope_create(Scope* outer, HashObject* locals) {
    Scope *scope = malloc(sizeof(Scope));
    *scope = (Scope) {
        .outer = outer,
        .locals = locals,
    };
    if (locals)
        INCREF(locals);
    return scope;
}

Scope*
Scope_leave(Scope* self) {
    if (self->locals)
        DECREF(self->locals);
    Scope* rv = self->outer;
    free(self);
    return rv;
}

#include "Objects/string.h"
#include "Objects/garbage.h"

static HashObject*
Scope_locate(Scope* self, Object* name) {
    // TODO: Use HashObject_lookup_fast();
    HashObject* table = self->locals;

    if (table && Bool_isTrue(table->base.type->contains((Object*) table, name)))
        return self->locals;

    Scope *scope = self->outer;
    if (scope)
        return Scope_locate(scope, name);

    return NULL;
}

Object*
Scope_lookup(Scope* self, Object* name) {
    HashObject* table = Scope_locate(self, name);
    if (!table)
        return NULL;

    return table->base.type->get_item((Object*) table, name);
}

void
Scope_assign_local(Scope* self, Object* name, Object* value) {
    HashObject* locals = self->locals;

    // Lazily setup locals dictionary
    if (!locals)
        locals = self->locals = Hash_new();

    locals->base.type->set_item((Object*) locals, name, value);
}

void
Scope_assign(Scope* self, Object* name, Object* value) {
    // Check local variables
    HashObject *table = Scope_locate(self, name);
    if (table)
         return table->base.type->set_item((Object*) table, name, value);

    return Scope_assign_local(self, name, value);
}