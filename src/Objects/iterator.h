#ifndef ITERATOR_H
#define ITERATOR_H

#include "object.h"

typedef struct lox_iterator {
    union {
        Object  object;
    };
    Object* (*next)(struct lox_iterator*);
    Object* previous;

    // For cleanup coordination of specific iterator type
    void (*cleanup)(Object*);
} Iterator;

Iterator* LoxIterator_create(size_t);

Object *LoxStopIteration;

#endif