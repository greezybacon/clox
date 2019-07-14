#ifndef LOX_RANGE_H
#define LOX_RANGE_H

#include "object.h"
#include "iterator.h"

typedef struct {
    union {
        Object      object;
    };
    Object      *start;
    Object      *end;
    Object      *step;
} LoxRange;

typedef struct {
    union {
        Object      object;
        Iterator    iterator;
    };
    Object      *current;
} LoxRangeIterator;

Object* LoxRange_create(Object*, Object*, Object*);

#endif