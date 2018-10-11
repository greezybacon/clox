#ifndef CLASS_H
#define CLASS_H

#include "object.h"
#include "hash.h"

typedef struct class_object {
    // Inherits from Object
    Object      base;

    HashObject  *attributes;
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
} BoundMethodObject;

ClassObject* Class_build(HashObject*);
Object* BoundMethod_create(Object *, Object *);

#endif