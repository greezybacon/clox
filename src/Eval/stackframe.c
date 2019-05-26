#include <assert.h>

#include "error.h"
#include "interpreter.h"
#include "scope.h"

#include "Include/Lox.h"
#include "Vendor/bdwgc/include/gc.h"

StackFrame*
StackFrame_create(StackFrame* previous) {
    StackFrame* frame = GC_MALLOC(sizeof(StackFrame));
    *frame = (StackFrame) {
        .prev = previous,
    };
    return frame;
}

Scope*
StackFrame_createScope(StackFrame* self) {
    // This assumes that, if anything is added to this frame's locals after
    // this closure is created, then those items should be visible within
    // the closure. Is that correct?
    if (!self->locals)
        self->locals = Hash_new();
    return Scope_create(self->scope, self->locals);
}

Object*
StackFrame_lookup(StackFrame *self, Object *key) {
    LoxTable *table = self->locals;
    Object *rv;

    if (table && NULL != (rv = Hash_getItem(table, key)))
        return rv;

    if ((rv = Scope_lookup(self->scope, key)))
        return rv;

    LoxString* skey = (LoxString*) key->type->as_string(key);
    eval_error(NULL, "%.*s: Variable has not yet been set in this scope\n", skey->length, skey->characters);

    return NULL;
}

void
StackFrame_assign_local(StackFrame *self, Object *name, Object *value) {
    LoxTable *table = self->locals;
    if (!table)
        table = self->locals = Hash_new();

    return table->base.type->set_item((Object*) table, name, value);
}

void
StackFrame_assign(StackFrame* self, Object* name, Object* value) {
    LoxTable *table = self->locals;
    if (table && table->base.type->contains((Object*) table, name))
        return StackFrame_assign_local(self, name, value);

    return Scope_assign(self->scope, name, value);
}

StackFrame*
StackFrame_push(Interpreter* self) {
    StackFrame* new = StackFrame_create(self->stack);
    return self->stack = new;
}

void
StackFrame_pop(Interpreter* self) {
    assert(self->stack != NULL);
    self->stack = self->stack->prev;
}
