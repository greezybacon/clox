#include <assert.h>
#include <stdio.h>

#include "Include/Lox.h"

static Object*
builtin_print(Interpreter* state, Object* self, Object* args) {
    assert(Tuple_isTuple(args));

    Object* arg;
    StringObject* sarg;
    size_t i = 0, argc = Tuple_getSize(args);

    for (; i < argc; i++) {
        arg = Tuple_getItem((TupleObject*) args, i);
        INCREF(arg);
        if (!String_isString(arg)) {
            sarg = String_fromObject(arg);
            DECREF(arg);
        }
        else
            sarg = (StringObject*) arg;

        fprintf(stdout, "%.*s", sarg->length, sarg->characters);
        DECREF(arg);
    }

    fprintf(stdout, "\n");
    return LoxNIL;
}

static Object*
builtin_int(Interpreter* state, Object* self, Object* args) {
    assert(Tuple_isTuple(args));

    size_t argc = Tuple_getSize(args);
    Object* arg = Tuple_getItem((TupleObject*) args, 0);

    if (arg->type->as_int) {
        return arg->type->as_int(arg);
    }

    // TODO: Raise error
    return LoxNIL;
}

static ModuleDescription
builtins_module_def = {
    .name = "__builtins__",
    .methods = {
        { "print", builtin_print },
        { "int", builtin_int },
        { 0 },
    }
};

ModuleObject*
BuiltinModule_init() {
    return (ModuleObject*) Module_init(&builtins_module_def);
}
