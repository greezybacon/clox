#include "error.h"
#include "interpreter.h"

#include "Objects/hash.h"

static Object*
eval_lookup(Interpreter* self, char* name, size_t length) {
    // TODO: Search local stack frame first
    
    // TODO: Cache the key in the AST node ...
    Object* key = (Object*) String_fromCharArrayAndSize(name, length);
    Object* value;

    HashObject* globals = self->globals;
    if ((value = ((Object*)globals)->type->get_item(globals, key))) {
        return value;
    }

    eval_error(self, "%s: Variable has not yet been set");
    return NULL;
}