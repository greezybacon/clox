#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Include/Lox.h"
#include "builtin.h"

#include "Objects/file.h"
#include "Objects/list.h"
#include "Objects/string.h"

static Object*
builtin_print(VmScope* state, Object* self, Object* args) {
    assert(Tuple_isTuple(args));

    Object* arg;
    LoxString* sarg;
    size_t i = 0, argc = Tuple_getSize(args);

    for (; i < argc; i++) {
        arg = Tuple_getItem((LoxTuple*) args, i);
        if (StringTree_isStringTree(arg)) {
            Object *chunk;
            Iterator *chunks = LoxStringTree_iterChunks((LoxStringTree*) arg);
            while (LoxStopIteration != (chunk = chunks->next(chunks))) {
                assert(String_isString(chunk));
                LoxString *schunk = (LoxString*) chunk;
                fprintf(stdout, "%.*s", schunk->length, schunk->characters);
            }
            LoxObject_Cleanup(chunks);
        }
        else {
            if (!String_isString(arg)) {
                sarg = String_fromObject(arg);
            }
            else
                sarg = (LoxString*) arg;

            INCREF(sarg);
            fprintf(stdout, "%.*s", sarg->length, sarg->characters);
            DECREF(sarg);
        }
    }

    fprintf(stdout, "\n");
    return LoxNIL;
}

static Object*
builtin_int(VmScope* state, Object* self, Object* args) {
    assert(Tuple_isTuple(args));

    Object* arg = Tuple_getItem((LoxTuple*) args, 0);

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
    Object* arg = Tuple_getItem((LoxTuple*) args, 0);

    if (arg->type->len) {
        return arg->type->len(arg);
    }

    // TODO: Raise error
    return LoxNIL;
}

static Object*
builtin_eval(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    char *text;
    int length;
    Lox_ParseArgs(args, "s#", &text, &length);

    VmScope scope = (VmScope) {
        .globals = state->globals,
    };

    Object *rv = vmeval_string_inscope(text, length, &scope);

    return rv;
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

    return (Object*) String_fromCharArrayAndSize(object->type->name,
        strlen(object->type->name));
}

static Object*
builtin_iter(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    Object *object;
    Lox_ParseArgs(args, "O", &object);

    if (object->type->iterate) {
        return (Object*) object->type->iterate(object);
    }
    else {
        fprintf(stderr, "Type `%s` is not iterable", object->type->name);
    }

    // TODO: Raise error
    return LoxNIL;
}

static Object*
builtin_list(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    LoxList* result = LoxList_new();

    if (Tuple_getSize(args) == 0)
        return (Object*) result;

    Object *object, *item;
    Lox_ParseArgs(args, "O", &object);

    if (object->type->iterate) {
        Iterator *items = object->type->iterate(object);
        while (LoxStopIteration != (item = items->next(items))) {
            LoxList_append(result, item);
        }
        LoxObject_Cleanup((Object*) items);
    }

    return (Object*) result;
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
        { "iter",   builtin_iter },
        { "list",   builtin_list },
        { 0 },
    }
};

LoxModule*
BuiltinModule_init() {
    return (LoxModule*) Module_init(&builtins_module_def);
}
