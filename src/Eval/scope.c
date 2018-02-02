#include <assert.h>

#include "scope.h"
#include "Objects/hash.h"

Scope*
Scope_create(Scope* outer) {
    Scope *scope = malloc(sizeof(Scope));
    *scope = (Scope) {
        .outer = outer,
    };
    return scope;
}

Scope*
Scope_leave(Scope* self) {
    free(self->locals);
    return self->outer;
}

static HashObject*
Scope_locate(Scope* self, Object* name) {
    // TODO: Use HashObject_lookup_fast();

    HashObject *table = self->locals;
    if (table && table->base.type->contains(table, name))
        return self->locals;

    Scope *scope = self;
    while (scope) {
        table = scope->locals;
        if (table->base.type->contains(table, name))
            return table;
        scope = scope->outer;
    }

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

    if (!locals) {
        // Lazily setup locals dictionary
        locals = self->locals = Hash_new();
    }

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