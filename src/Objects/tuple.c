#include <assert.h>
#include <stdarg.h>

#include "tuple.h"
#include "integer.h"
#include "garbage.h"

static struct object_type TupleType;

TupleObject*
Tuple_fromArgs(int count, ...) {
    va_list args;

    TupleObject* self = object_new(sizeof(TupleObject), &TupleType);
    self->count = count;

    if (count > 0) {
        va_start(args, count);
        Object** items = malloc(count * sizeof(Object*));
        Object** pitem = items;
        Object* item;
        while (count--) {
            *(pitem++) = item = va_arg(args, Object*);
            INCREF(item);
        }
        va_end(args);
        self->items = items;
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