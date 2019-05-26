#include <assert.h>
#include <stdio.h>

#include "scope.h"
#include "Objects/boolean.h"
#include "Objects/hash.h"

Scope*
Scope_create(Scope* outer, LoxTable* locals) {
    Scope *scope = malloc(sizeof(Scope));
    *scope = (Scope) {
        .outer = outer,
        .locals = locals,
    };
    return scope;
}

Scope*
Scope_leave(Scope* self) {
    Scope* rv = self->outer;
    free(self);
    return rv;
}

#include "Objects/string.h"

static LoxTable*
Scope_locate(Scope* self, Object* name) {
    LoxTable* table = self->locals;
    Object* rv;

    if (table && Hash_contains(table, name))
        return table;

    Scope *scope = self->outer;
    if (scope)
        return Scope_locate(scope, name);

    return NULL;
}

Object*
Scope_lookup(Scope* self, Object* name) {
    // TODO: Use LoxTable_lookup_fast();
    LoxTable* table = self->locals;
    Object* rv;

    if (table && NULL != (rv = Hash_getItem(table, name)))
        return rv;

    Scope *scope = self->outer;
    if (scope)
        return Scope_lookup(scope, name);

    return NULL;
}

void
Scope_assign_local(Scope* self, Object* name, Object* value) {
    LoxTable* locals = self->locals;

    // Lazily setup locals dictionary
    if (!locals)
        locals = self->locals = Hash_new();

    locals->base.type->set_item((Object*) locals, name, value);
}

void
Scope_assign(Scope* self, Object* name, Object* value) {
    // Check local variables
    LoxTable *table = Scope_locate(self, name);
    if (table)
         return table->base.type->set_item((Object*) table, name, value);

    return Scope_assign_local(self, name, value);
}