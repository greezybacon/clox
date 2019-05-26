#ifndef INTEGER_H
#define INTEGER_H

#include "object.h"

typedef struct integer_object {
    // Inherits from Object
    Object  base;

    long long value;
} LoxInteger;

LoxInteger* Integer_fromLongLong(long long value);
long long Integer_toInt(Object*);
bool Integer_isInteger(Object*);
Object* Integer_fromObject(Object*);

#endif

