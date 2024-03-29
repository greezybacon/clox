#ifndef CLASS_H
#define CLASS_H

#include "object.h"
#include "hash.h"
#include "module.h"

typedef struct class_object {
    // Inherits from Object
    Object      base;

    Object      *name;
    LoxTable    *attributes;
    struct class_object *parent;
} LoxClass;

typedef struct instance_object {
    Object      base;

    LoxClass    *class;
    LoxTable    *attributes;
} LoxInstance;

typedef struct boundmethod_object {
    Object      base;

    Object      *method;
    Object      *object;
    LoxClass    *origin;
} LoxBoundMethod;

LoxClass* Class_build(LoxTable*, LoxClass*);
Object* BoundMethod_create(Object *, Object *);
bool Class_isClass(Object*);
LoxClass* LoxClass_fromModuleDescriptionAndParent(ModuleDescription *, LoxClass *);
LoxClass* LoxClass_fromModuleDescription(ModuleDescription *);

void LoxInstance_setAttribute(Object *, Object *, Object *);
Object* LoxInstance_getAttribute(Object *, Object *);

#endif