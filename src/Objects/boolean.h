#ifndef BOOLEAN_H
#define BOOLEAN_H

#include <stdbool.h>
#include "object.h"

typedef struct bool_object {
    // Inherits from Object
    Object  base;
    bool value;
} BoolObject;

BoolObject* Bool_fromBool(bool);
BoolObject* Bool_fromObject(Object*);
bool Bool_isBool(Object*);
bool Bool_isTrue(Object*);
#define Bool_ISTRUE(value) (((BoolObject*) (value)) == LoxTRUE)

BoolObject *LoxTRUE, *LoxFALSE;
Object *LoxNIL;
Object *LoxUndefined;

#endif