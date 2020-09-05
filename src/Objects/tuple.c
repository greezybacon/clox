#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "tuple.h"
#include "integer.h"
#include "string.h"
#include "Vendor/bdwgc/include/gc.h"

static struct object_type TupleType;

LoxTuple*
Tuple_new(size_t count) {
    LoxTuple* self = object_new(sizeof(LoxTuple), &TupleType);
    self->count = count;
    self->items = malloc(count * sizeof(Object*));
    return self;
}

LoxTuple*
Tuple_fromArgs(size_t count, ...) {
    va_list args;

    LoxTuple* self = Tuple_new(count);

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

LoxTuple*
Tuple_fromList(size_t count, Object **first) {
    LoxTuple* self = Tuple_new(count);

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
Tuple_getItem(LoxTuple* self, int index) {
    if (index < 0)
        index += self->count;
    if (self->count < index + 1) {
        // Raise error
        return LoxNIL;
    }

    return *(self->items + index);
}

void
Tuple_setItem(LoxTuple* self, size_t index, Object* value) {
    if (self->count > index) {
        DECREF(*(self->items + index));
        *(self->items + index) = value;
    }
    // Else raise error
}

size_t
Tuple_getSize(Object* self) {
    assert(self->type == &TupleType);

    return ((LoxTuple*) self)->count;
}

bool
Tuple_isTuple(Object* self) {
    return self->type == &TupleType;
}

static Object*
tuple_entries__next(Iterator *self) {
    TupleIterator *this = (TupleIterator*) self;
    LoxTuple* target = (LoxTuple*) self->target;

    if (this->pos < target->count)
        return *(target->items + this->pos++);

    return LoxStopIteration;
}

Iterator*
Tuple_getIterator(LoxTuple* self) {
    TupleIterator* it = (TupleIterator*) LoxIterator_create((Object*) self, sizeof(TupleIterator));

    it->iterator.next = tuple_entries__next;

    return (Iterator*) it;
}

static Object*
tuple_getitem(Object* self, Object* index) {
    assert(self->type == &TupleType);

    int idx = Integer_toInt(index);
    return Tuple_getItem((LoxTuple*) self, idx);
}

static Object*
tuple_len(Object* self) {
    assert(self->type == &TupleType);
    return (Object*) Integer_fromLongLong(((LoxTuple*) self)->count);
}

static Object*
tuple_asstring(Object* self) {
    assert(self->type == &TupleType);

    char buffer[1024];  // TODO: Use the + operator of LoxString
    char* position = buffer;
    int remaining = sizeof(buffer) - 1, bytes;

    bytes = snprintf(position, remaining, "(");
    position += bytes, remaining -= bytes;

    Object *value;
    LoxString *svalue;
    LoxTuple *this = (LoxTuple*) self;

    int k = this->count, j = k - 1, i = 0;
    for (; i < k; i++) {
        value = *(this->items + i);
        svalue = (LoxString*) value->type->as_string(value);
        bytes = snprintf(position, remaining, "%.*s%s",
            svalue->length, svalue->characters,
            i == j ? "" : ", ");
        position += bytes, remaining -= bytes;
    }
    position += snprintf(position, remaining, ")");

    return (Object*) String_fromCharsAndSize(buffer, position - buffer);
}

static hashval_t
tuple_hash(Object *self) {
    assert(self->type == &TupleType);
    LoxTuple *this = (LoxTuple*) self;

    int i = this->count;
    hashval_t result = i, tmp;
    Object *item;
    while (i--) {
        item = *(this->items + i);
        if (item->type->hash)
            tmp = item->type->hash(item);
        else
            tmp = MYADDRESS(item);

        result   = (tmp << 11) + result;
        result  += result >> 11;
    }

    result ^= result << 3;
    result += result >> 5;
    result ^= result << 4;
    result += result >> 17;
    result ^= result << 25;
    result += result >> 6;

    return result;
}

static Iterator*
tuple_iterate(Object *self) {
    assert(self->type == &TupleType);
    return Tuple_getIterator((LoxTuple*) self);
}

static void
tuple_cleanup(Object *self) {
    assert(self->type == &TupleType);

    LoxTuple *this = (LoxTuple*) self;
    Object** pitem = this->items;
    while (this->count--)
        DECREF(*(pitem++));

    free(this->items);
}

static int
tuple_compare(Object *self, Object *other) {
    if (other->type != &TupleType)
        return -1;

    int count = Tuple_getSize(self), rv = count - Tuple_getSize(other);
    if (rv != 0)
        return rv;

    Object *item, *oitem;
    int i = 0;
    while (i < count) {
        item = Tuple_GETITEM(self, i);
        oitem = Tuple_GETITEM(other, i);
        if (likely(item->type->compare != NULL)) {
            if ((rv = item->type->compare(item, oitem)) != 0)
                return rv;
        }
        else {
            // Undefined?
            return -1;
        }
        i++;
    }

    return 0;
}

static struct object_type TupleType = (ObjectType) {
    .name = "tuple",
    .len = tuple_len,
    .hash = tuple_hash,
    .get_item = tuple_getitem,
    .iterate = tuple_iterate,
    .compare = tuple_compare,

    .as_string = tuple_asstring,
    .cleanup = tuple_cleanup,
};

static LoxTuple _LoxEmptyTuple = (LoxTuple) {
    .base = (Object) {
        .type = &TupleType,
        .refcount = 1,
    },
    .count = 0,
    .items = NULL,
};
const LoxTuple *LoxEmptyTuple = &_LoxEmptyTuple;
