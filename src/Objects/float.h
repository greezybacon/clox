#ifndef FLOAT_H
#define FLOAT_H

#include "object.h"

typedef struct float_object {
    // Inherits from Object
    Object  base;

    long double value;
} FloatObject;

FloatObject* Float_fromLongDouble(long double);
FloatObject* Float_fromLongLong(long long);
Object* Float_fromObject(Object*);
bool Float_isFloat(Object*);
long double Float_toLongDouble(Object*);

#endif

