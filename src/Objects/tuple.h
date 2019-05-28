#ifndef TUPLE_H
#define TUPLE_H

#include <stdbool.h>

#include "iterator.h"
#include "object.h"

typedef struct tuple_object {
    // Inherits from Object
    Object      base;

    int         count;
    Object**    items;
} LoxTuple;

typedef struct {
    union {
        Object      object;
        Iterator    iterator;
    };
    int         pos;
} TupleIterator;

LoxTuple* Tuple_new(size_t);
LoxTuple* Tuple_fromArgs(size_t, ...);
LoxTuple* Tuple_fromList(size_t, Object**);
Object* Tuple_getItem(LoxTuple*, int);
void Tuple_setItem(LoxTuple*, size_t, Object*);
bool Tuple_isTuple(Object*);
size_t Tuple_getSize(Object*);

#define Tuple_GETITEM(self, index) *(((LoxTuple*) self)->items + index)

const LoxTuple *LoxEmptyTuple;
#endif
