#ifndef ITERATOR_H
#define ITERATOR_H

#include "object.h"

typedef struct lox_iterator {
    Object* (*next)(struct lox_iterator*);
    Object* previous;
} Iterator;

#endif