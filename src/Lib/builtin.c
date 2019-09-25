#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Include/Lox.h"
#include "builtin.h"

#include "Objects/file.h"
#include "Objects/integer.h"
#include "Objects/list.h"
#include "Objects/range.h"
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
            INCREF(chunks);
            while (LoxStopIteration != (chunk = chunks->next(chunks))) {
                assert(String_isString(chunk));
                LoxString *schunk = (LoxString*) chunk;
                fprintf(stdout, "%.*s", schunk->length, schunk->characters);
            }
            DECREF(chunks);
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

    Object *rv = LoxVM_evalStringWithScope(text, length, &scope);

    free(text);
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

    Object *rv = (Object*) Lox_FileOpen(filename, flags);

    free(filename);
    free(flags);
    return rv;
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

    Object *object=NULL, *item;
    Lox_ParseArgs(args, "|O", &object);

    if (object && object->type->iterate) {
        INCREF(object);
        Iterator *it = object->type->iterate(object);
        INCREF(it);
        while (LoxStopIteration != (item = it->next(it))) {
            LoxList_append(result, item);
        }
        DECREF(it);
        DECREF(object);
    }

    return (Object*) result;
}

static Object*
builtin_range(VmScope *state, Object *self, Object *args) {
    assert(Tuple_isTuple(args));

    Object *start=NULL, *end, *step=NULL;
    Lox_ParseArgs(args, "O|OO", &end, &start, &step);

    if (start != NULL) {
        Object *T = start;
        start = end;
        end = T;
    }
    else {
        LoxInteger *zero = Integer_fromLongLong(0);
        start = (Object*) end->type->op_star(end, (Object*) zero);
        LoxObject_Cleanup((Object*) zero);
    }

    if (step == NULL) {
        step = (Object*) Integer_fromLongLong(1);
    }

    return LoxRange_create(start, end, step);
}

static Object*
builtin_import(VmScope *state, Object *self, Object *args) {
    char *filename;
    Lox_ParseArgs(args, "s", &filename);

    return LoxModule_FindAndImport(filename);
}

static Object*
builtin_globals(VmScope *state, Object *self, Object *args) {
    assert(state);

    if (state->outer)
        return (Object*) state->outer->globals;

    return (Object*) state->globals;
}

static Object*
builtin_sum(VmScope *state, Object *self, Object *args) {
    Object *iterable, *initial=NULL, *next, *value;
    Lox_ParseArgs(args, "O|O", &iterable, &initial);

    if (!iterable->type->iterate)
        return LoxUndefined;

    Iterator *it = iterable->type->iterate(iterable);
    INCREF(it);
    if (initial == NULL)
        initial = (Object*) Integer_fromLongLong(0);

    while (LoxStopIteration != (next = it->next(it))) {
        INCREF(initial);
        INCREF(next);
        value = initial->type->op_plus(initial, next);
        DECREF(initial);
        DECREF(next);
        initial = value;
    }
    DECREF(it);

    return initial;
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
        { "range",  builtin_range },
        { "import", builtin_import },
        { "sum",    builtin_sum },
        { "globals", builtin_globals },
        { 0 },
    }
};

LoxModule*
BuiltinModule_init() {
    return (LoxModule*) Module_init(&builtins_module_def);
}
