#include <assert.h>
#include <stdio.h>

#include "Include/Lox.h"
#include "builtin.h"

#include "Objects/file.h"

static Object*
builtin_print(VmScope* state, Object* self, Object* args) {
    assert(Tuple_isTuple(args));

    Object* arg;
    StringObject* sarg;
    size_t i = 0, argc = Tuple_getSize(args);

    for (; i < argc; i++) {
        arg = Tuple_getItem((TupleObject*) args, i);
        if (!String_isString(arg)) {
            sarg = String_fromObject(arg);
        }
        else
            sarg = (StringObject*) arg;

        fprintf(stdout, "%.*s", sarg->length, sarg->characters);
    }

    fprintf(stdout, "\n");
    return LoxNIL;
}

static Object*
builtin_int(VmScope* state, Object* self, Object* args) {
    assert(Tuple_isTuple(args));

    Object* arg = Tuple_getItem((TupleObject*) args, 0);

    if (arg->type->as_int) {
        return arg->type->as_int(arg);
    }

    // TODO: Raise error
    return LoxNIL;
}

static Object*
builtin_len(VmScope* state, Object* self, Object* args) {
    assert(Tuple_isTuple(args));

    size_t argc = Tuple_getSize(args);
    Object* arg = Tuple_getItem((TupleObject*) args, 0);

    if (arg->type->len) {
        return arg->type->len(arg);
    }

    // TODO: Raise error
    return LoxNIL;
}

static Object*
builtin_eval(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    size_t argc = Tuple_getSize(args);
    if (argc != 1)
        printf("eval() takes exactly one argument\n");

    Object* arg = Tuple_getItem((TupleObject*) args, 0);
    StringObject *string;
    if (!String_isString(arg)) {
        if (arg->type->as_string)
            string = (StringObject*) arg->type->as_string(arg);
        else
            // Error
            printf("eval: cannot coerce argument to string\n");
    }
    else {
        string = (StringObject*) arg;
    }

    VmScope scope = (VmScope) {
        .globals = state->globals,
    };
    return vmeval_string_inscope(string->characters, string->length, &scope);
}

static Object*
builtin_tuple(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));
    return args;
}

static Object*
builtin_table(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    // TODO: Consider value of args as list of two-item tuples?
    return (Object*) Hash_new();
}

static Object*
builtin_open(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    size_t argc = Tuple_getSize(args);

    char *filename, *flags;
    Lox_ParseArgs(args, "ss", &filename, &flags);

    return (Object*) Lox_FileOpen(filename, flags);
}

static Object*
builtin_hash(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    Object *object;
    Lox_ParseArgs(args, "O", &object);

    return (Object*) Integer_fromLongLong(HASHVAL(object));
}

static Object*
builtin_format(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    Object *object;
    char *format;
    Lox_ParseArgs(args, "Os", &object, &format);
    INCREF(object);

    Object *rv = LoxObject_Format(object, format);

    DECREF(object);
    free(format);

    return rv;
}

static Object*
builtin_type(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    Object *object;
    Lox_ParseArgs(args, "O", &object);

    return String_fromCharArrayAndSize(object->type->name, strlen(object->type->name));
}

static ModuleDescription
builtins_module_def = {
    .name = "__builtins__",
    .methods = {
        { "print",  builtin_print },
        { "int",    builtin_int },
        { "len",    builtin_len },
        { "eval",   builtin_eval },
        { "tuple",  builtin_tuple },
        { "table",  builtin_table, },
        { "open",   builtin_open },
        { "hash",   builtin_hash },
        { "format", builtin_format },
        { "type",   builtin_type },
        { 0 },
    }
};

ModuleObject*
BuiltinModule_init() {
    return (ModuleObject*) Module_init(&builtins_module_def);
}
