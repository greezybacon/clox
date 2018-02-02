#include <assert.h>

#include "error.h"
#include "interpreter.h"

#include "Include/Lox.h"

StackFrame*
StackFrame_new(void) {
    StackFrame* frame = calloc(1, sizeof(StackFrame));
    return frame;
}

Object*
stack_lookup2(Interpreter* self, Object* key) {
    Object *value = Scope_lookup(self->scope, key);
    if (value)
        return value;

    StringObject* skey = (StringObject*) key->type->as_string(key);
    eval_error(self, "%.*s: Variable has not yet been set in this scope\n", skey->length, skey->characters);
    DECREF((Object*) skey);

    return NULL;
}

Object*
stack_lookup(Interpreter* self, char* name, size_t length) {
    // TODO: Cache the key in the AST node ...
    Object* key = (Object*) String_fromCharArrayAndSize(name, length);
    Object* rv = stack_lookup2(self, key);
    DECREF(key);

    return rv;
}

void
stack_assign2(Interpreter* self, Object* name, Object* value) {
    return Scope_assign(self->scope, key, value);
}

void
StackFrame_push(Interpreter* self) {
    StackFrame* new = StackFrame_new();
    new->prev = self->stack;

    self->stack = new;
}

void
StackFrame_pop(Interpreter* self) {
    assert(self->stack != NULL);

    free(self->stack);
    self->stack = self->stack->prev;
}
