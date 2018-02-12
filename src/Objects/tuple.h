#ifndef TUPLE_H
#define TUPLE_H

#include <stdbool.h>

#include "object.h"

typedef struct tuple_object {
    // Inherits from Object
    Object      base;

    int         count;
    Object**    items;
} TupleObject;

TupleObject* Tuple_new(size_t);
TupleObject* Tuple_fromArgs(size_t, ...);
Object* Tuple_getItem(TupleObject*, int);
void Tuple_setItem(TupleObject*, size_t, Object*);
bool Tuple_isTuple(Object*);
size_t Tuple_getSize(Object*);

#endif
