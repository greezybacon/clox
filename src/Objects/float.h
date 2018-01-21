#ifndef FLOAT_H
#define FLOAT_H

#include "object.h"

typedef struct float_object {
    // Inherits from Object
    Object  base;

    long double value;
} FloatObject;

FloatObject*
Float_fromLongDouble(long double value);

FloatObject*
Float_fromLongLong(long long value);

#endif

