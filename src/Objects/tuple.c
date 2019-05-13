#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "tuple.h"
#include "integer.h"
#include "string.h"
#include "Vendor/bdwgc/include/gc.h"

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

TupleObject*
Tuple_fromList(size_t count, Object **first) {
    TupleObject* self = Tuple_new(count);

    if (count > 0) {
        Object** pitem = self->items;
        Object* item;
        while (count--) {
            *(pitem++) = item = *first++;
            INCREF(item);
        }
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
        DECREF(*(self->items + index));
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

static Object*
tuple_len(Object* self) {
    assert(self->type == &TupleType);
    return (Object*) Integer_fromLongLong(((TupleObject*) self)->count);
}

static Object*
tuple_asstring(Object* self) {
    assert(self->type == &TupleType);

    char buffer[1024];  // TODO: Use the + operator of StringObject
    char* position = buffer;
    int remaining = sizeof(buffer) - 1, bytes;

    bytes = snprintf(position, remaining, "(");
    position += bytes, remaining -= bytes;

    Object *value;
    StringObject *svalue;
    TupleObject *this = (TupleObject*) self;

    int k = this->count, j = k - 1, i = 0;
    for (; i < k; i++) {
        value = *(this->items + i);
        svalue = (StringObject*) value->type->as_string(value);
        bytes = snprintf(position, remaining, "%.*s%s",
            svalue->length, svalue->characters,
            i == j ? "" : ", ");
        position += bytes, remaining -= bytes;
    }
    position += snprintf(position, remaining, ")");

    return (Object*) String_fromCharArrayAndSize(buffer, position - buffer);
}

static void
tuple_cleanup(Object *self) {
    assert(self->type == &TupleType);

    TupleObject *this = (TupleObject*) self;
    Object** pitem = this->items;
    while (this->count--)
        DECREF(*(pitem++));

    free(this->items);
}

static struct object_type TupleType = (ObjectType) {
    .name = "tuple",
    .len = tuple_len,
    .get_item = tuple_getitem,

    .as_string = tuple_asstring,
    .cleanup = tuple_cleanup,
};

static TupleObject _LoxEmptyTuple = (TupleObject) {
    .base = (Object) {
        .type = &TupleType,
        .refcount = 1,
    },
    .count = 0,
    .items = NULL,
};
const TupleObject *LoxEmptyTuple = &_LoxEmptyTuple;
