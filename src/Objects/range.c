#include <assert.h>

#include "Compile/vm.h"

#include "iterator.h"
#include "object.h"
#include "range.h"

static struct object_type LoxRangeType;

static Object*
loxrange_next(Iterator *self) {
    LoxRange *range = (LoxRange*) self->target;
    LoxRangeIterator *iter = (LoxRangeIterator*) self;

    // XXX: Add some assertions
    Object **current = &iter->current;
    *current = (*current)->type->op_plus(*current, range->step);
    if ((*current)->type->compare(*current, range->end) < 0)
        return *current;

    return LoxStopIteration;
}

Object*
LoxRange_create(Object *start, Object *stop, Object *step) {
    LoxRange *range = object_new(sizeof(LoxRange), &LoxRangeType);
    range->start = start;
    range->end = stop;
    range->step = step;

    return (Object*) range;
}

Iterator*
range_iterate(Object *self) {
    assert(self->type == &LoxRangeType);

    LoxRangeIterator* iter = (LoxRangeIterator*) LoxIterator_create(self, sizeof(LoxRangeIterator));
    iter->iterator.next = loxrange_next;
    iter->current = ((LoxRange*) self) ->start;
    return (Iterator*) iter;
}

static struct object_type LoxRangeType = (ObjectType) {
    .name = "range",
    .iterate = range_iterate,

    //.as_string = range_asstring,
};
