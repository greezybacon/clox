#ifndef BOOLEAN_H
#define BOOLEAN_H

#include <stdbool.h>
#include "object.h"

typedef struct bool_object {
    // Inherits from Object
    Object  base;
    bool value;
} BoolObject;

Object* Bool_fromBool(bool);
Object* Bool_fromObject(Object*);
bool Bool_isBool(Object*);

BoolObject *LoxTRUE, *LoxFALSE;
Object *LoxNIL;

#endif