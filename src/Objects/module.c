#include <assert.h>
#include <libgen.h>
#include <string.h>

#include "Compile/vm.h"

#include "boolean.h"
#include "function.h"
#include "hash.h"
#include "iterator.h"
#include "module.h"
#include "object.h"
#include "string.h"
#include "tuple.h"

static struct object_type ModuleType;

Object*
Module_init(ModuleDescription* desc) {
    LoxModule* self = object_new(sizeof(LoxModule), &ModuleType);
    self->name = (Object*) String_fromCharsAndSize(desc->name, strlen(desc->name));
    INCREF(self->name);
    self->properties = Hash_new();
    INCREF(self->properties);

    LoxModule_buildProperties(desc->properties, self->properties);

    return (Object*) self;
}

int
LoxModule_buildProperties(ObjectProperty *properties, LoxTable *table) {
    LoxString* name;
    Object* value;
    ObjectProperty* M = properties;

    while (M->name) {
        name = String_fromCharsAndSize(M->name, strlen(M->name));
        if (M->type == OBJECT_PROP_TYPE_METHOD) {
            value = (Object*) NativeFunction_new(M->method);
        }
        else if (M->type == OBJECT_PROP_TYPE_PROPERTY) {
            value = (Object*) LoxNativeProperty_create(M->property.getter, M->property.setter, NULL);
        }
        Hash_setItem(table, (Object*) name, value);
        M++;
    }

    return 0;
}

static Object*
module_getitem(Object* self, Object* name) {
    assert(self->type == &ModuleType);

    LoxModule* module = (LoxModule*) self;
    Object *value = Hash_getItem(module->properties, name);

    if (LoxNativeProperty_isProperty(value)) {
        value = ((LoxNativeProperty*) value)->getter;
    }

    return value;
}

static LoxBool*
module_contains(Object* self, Object* name) {
    assert(self->type == &ModuleType);

    LoxModule* module = (LoxModule*) self;
    return Bool_fromBool(Hash_contains(module->properties, name));
}

static struct object_type ModuleType = (ObjectType) {
    .name = "module",
    
    .get_item = module_getitem,
    .contains = module_contains,
};

// Utility API functions
static LoxTable*
import_file(FILE *text, const char *name) {
    // Now, eval the compiled code in a new context to capture the defined "globals"
    LoxTable *globals = Hash_new();
    VmScope scope = (VmScope) {
        .globals = globals,
    };
    LoxVM_evalFileWithScope(text, name, &scope);

    // TODO: Place the module in a global scope as a cache

    return globals;
}

bool
LoxModule_isModule(Object *object) {
   return object->type == &ModuleType;
}

Object*
LoxModule_ImportFile(const char *absolute_path, const char *name) {
    FILE *text = fopen(absolute_path, "r");
    if (text == NULL) {
        // TODO: Throw an exception
        return NULL;
    }

    LoxTable *properties = import_file(text, name);
    fclose(text);

    // TODO: Create bonafide Module object
    LoxModule* module = object_new(sizeof(LoxModule), &ModuleType);
    module->name = (Object*) String_fromCharsAndSize(name, strlen(name));
    INCREF(module->name);
    module->properties = properties;

    return (Object*) module;
}

Object*
LoxModule_ImportFileWithSearchPath(const char * module_name, Object * search) {
    char abs_path[256]; // XXX: Lookup max_path
    Object *module = NULL;
    if (search->type->iterate) {
        Iterator *it = search->type->iterate(search);
        Object *prefix;
        LoxString *S;
        while (LoxStopIteration != (prefix = it->next(it))) {
            S = String_fromObject(prefix);
            INCREF(S);
            snprintf(abs_path, sizeof(abs_path), "%.*s/%s", S->length, S->characters, module_name);
            DECREF(S);
            module = LoxModule_ImportFile(abs_path, module_name);
            if (module != NULL)
                return module;
        }
    }
    fprintf(stderr, "WARNING: Cannot locate requested module\n");
    return LoxUndefined;
}

Object*
LoxModule_FindAndImport(const char * module_name) {
    // XXX: Assume relative import
    static LoxTuple *search = NULL;
    if (search == NULL) {
        LoxString *dot = String_fromConstant(".");
        INCREF(dot);
        search = Tuple_fromArgs(1, (Object*) dot);
        INCREF(search);
    }

    if (module_name[0] != '/') {
        // TODO: Use some system-based import path list?
        return LoxModule_ImportFileWithSearchPath(module_name, (Object*) search);
    }
    else {
        return LoxModule_ImportFile(module_name, basename(module_name));
    }
}