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

typedef struct {
    union {
        Object      object;
    };
    long long   start;
    long long   end;
    long long   step;
} LoxIntRange;

typedef struct {
    union {
        Object      object;
        Iterator    iterator;
    };
    long long   current;
} LoxIntRangeIterator;

Object* LoxRange_create(Object*, Object*, Object*);

#endif