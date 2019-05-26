#include <assert.h>
#include <string.h>

#include "boolean.h"
#include "function.h"
#include "hash.h"
#include "module.h"
#include "object.h"
#include "string.h"

static struct object_type ModuleType;

Object*
Module_init(ModuleDescription* desc) {
    LoxModule* self = object_new(sizeof(LoxModule), &ModuleType);
    self->name = (Object*) String_fromCharArrayAndSize(desc->name, strlen(desc->name));
    self->properties = Hash_new();

    ObjectMethod* M = desc->methods;
    LoxString* name;
    Object* method;
    while (M->name) {
        name = String_fromCharArrayAndSize(M->name, strlen(M->name));
        method = (Object*) NativeFunction_new(M->method);
        Hash_setItem(self->properties, (Object*) name, method);
        M++;
    }

    return (Object*) self;
}

static Object*
module_getitem(Object* self, Object* name) {
    assert(self->type == &ModuleType);

    LoxModule* module = (LoxModule*) self;
    return Hash_getItem(module->properties, name);
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