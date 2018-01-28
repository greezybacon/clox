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

BoolObject *LoxTRUE, *LoxFALSE;

#endif