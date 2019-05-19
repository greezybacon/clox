#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "object.h"
#include "iterator.h"
#include "string.h"

static struct object_type IteratorType;

Iterator*
LoxIterator_create(size_t sizeof_derived_type) {
    return object_new(sizeof_derived_type, &IteratorType);
}

Object*
iterator_next(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &IteratorType);
    
    Iterator *this = (Iterator*) self;
    return this->next(this);
}

static struct object_type IteratorType = (ObjectType) {
    .code = TYPE_ITERATOR,
    .name = "iterator",
    .hash = MYADDRESS,
    .op_eq = IDENTITY,
    
    .methods = (ObjectMethod[]) {
        { "next", iterator_next },
        { 0, 0 },
    },
};

static Object*
iterstop_asstring(Object* self) {
    return (Object*) String_fromConstant("stop-iteration");
}

static struct object_type StopIterationType = (ObjectType) {
    .code = TYPE_UNDEFINED,
    .name = "stop-iteration",

    .as_string = iterstop_asstring,

    .op_eq = IDENTITY,
    .hash = MYADDRESS,
    .cleanup = ERROR,
};

static Object _LoxStopIteration = (Object) {
    .type = &StopIterationType,
    .refcount = 1,
};
Object *LoxStopIteration = &_LoxStopIteration;