#include <assert.h>
#include <stdarg.h>

#include "tuple.h"
#include "integer.h"
#include "garbage.h"

static struct object_type TupleType;

TupleObject*
Tuple_new(size_t count) {
    TupleObject* self = object_new(sizeof(TupleObject), &TupleType);
    self->count = count;
    self->items = malloc(count * sizeof(Object*));
    return self;      
}

TupleObject*
Tuple_fromArgs(size_t count, ...) {
    va_list args;

    TupleObject* self = Tuple_new(count);

    if (count > 0) {
        va_start(args, count);
        Object** pitem = self->items;
        Object* item;
        while (count--) {
            *(pitem++) = item = va_arg(args, Object*);
            INCREF(item);
        }
        va_end(args);
    }
    return self;
}

Object*
Tuple_getItem(TupleObject* self, int index) {
    if (index < 0)
        index += self->count;
    if (self->count < index + 1) {
        // Raise error
        return NULL;
    }

    return *(self->items + index);
}

void
Tuple_setItem(TupleObject* self, size_t index, Object* value) {
    if (self->count > index) {
        INCREF(value);
        *(self->items + index) = value;
    }
    // Else raise error
}

size_t
Tuple_getSize(Object* self) {
    assert(self->type == &TupleType);

    return ((TupleObject*) self)->count;
}

bool
Tuple_isTuple(Object* self) {
    return self->type == &TupleType;
}

static Object*
tuple_getitem(Object* self, Object* index) {
    assert(self->type == &TupleType);

    int idx = Integer_toInt(index);
    return Tuple_getItem((TupleObject*) self, idx);
}

static void
tuple_cleanup(Object* self) {
    assert(self->type == &TupleType);
    
    TupleObject* this = (TupleObject*) self;
    int i = this->count;
    while (i)
        DECREF(*(this->items + --i));

    free(this->items);
}

static struct object_type TupleType = (ObjectType) {
    .name = "tuple",
    .get_item = tuple_getitem,
    .cleanup = tuple_cleanup,
};