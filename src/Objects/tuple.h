#ifndef TUPLE_H
#define TUPLE_H

#include "object.h"

typedef struct tuple_object {
    // Inherits from Object
    Object      base;

    int         count;
    Object**    items;
} TupleObject;

TupleObject* Tuple_fromArgs(int, ...);
Object* Tuple_getItem(TupleObject*, int);

#endif
