#ifndef CLASS_H
#define CLASS_H

#include "object.h"
#include "hash.h"

typedef struct class_object {
    // Inherits from Object
    Object      base;

    Object      *name;
    HashObject  *attributes;
    struct class_object *parent;
} ClassObject;

typedef struct instance_object {
    Object      base;

    ClassObject *class;
    HashObject  *attributes;
} InstanceObject;

typedef struct boundmethod_object {
    Object      base;

    Object      *method;
    Object      *object;
    ClassObject *origin;
} BoundMethodObject;

ClassObject* Class_build(HashObject*, ClassObject*);
Object* BoundMethod_create(Object *, Object *);

#endif