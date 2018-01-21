#ifndef INTEGER_H
#define INTEGER_H

#include "object.h"

typedef struct integer_object {
    // Inherits from Object
    Object  base;

    long long value;
} IntegerObject;

IntegerObject*
Integer_fromLongLong(long long value);

#endif

