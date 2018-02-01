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
    // TODO: Use HashObject_lookup_fast();
    Object *value;
    HashObject *sources[] = { self->stack->locals, self->globals };
    Object *table = (Object*) sources[0];
    int i = 0, j;

    for (j = (sizeof(sources) / sizeof(table)) - 1; j > 0; j--, table++)
        if (table != NULL)
            if ((value = ((Object*) table)->type->get_item((Object*) table, key)))
                return value;

    StringObject* skey = (StringObject*) key->type->as_string(key);
    eval_error(self, "%.*s: Variable has not yet been set", skey->length, skey->characters);
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
    HashObject* locals = self->stack->locals;

    if (!locals) {
        // Lazily setup locals dictionary
        locals = self->stack->locals = Hash_new();
    }

    ((Object*) locals)->type->set_item((Object*) locals, name, value);
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

    HashObject* locals = self->stack->locals;
    if (locals)
        DECREF((Object*) locals);

    free(self->stack);
    self->stack = self->stack->prev;
}