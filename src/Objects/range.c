#include <assert.h>

#include "Compile/vm.h"

#include "integer.h"
#include "iterator.h"
#include "object.h"
#include "range.h"

static struct object_type LoxRangeType;
static struct object_type LoxIntRangeType;

Object*
LoxRange_create(Object *start, Object *stop, Object *step) {
    if (Integer_isInteger(start) && Integer_isInteger(stop) && Integer_isInteger(step)) {
        LoxIntRange *range = object_new(sizeof(LoxIntRange), &LoxIntRangeType);
        range->start = Integer_toInt(start);
        range->end = Integer_toInt(stop);
        range->step = Integer_toInt(step);
        return (Object*) range;
    }

    LoxRange *range = object_new(sizeof(LoxRange), &LoxRangeType);
    range->start = start;
    range->end = stop;
    range->step = step;

    return (Object*) range;
}

// A generic range object which also supports floats

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

// A specific range object optimized for integer operations

static Object*
loxrange_next_int(Iterator *self) {
    LoxIntRange *range = (LoxIntRange*) self->target;
    LoxIntRangeIterator *iter = (LoxIntRangeIterator*) self;

    long long *current = &iter->current;
    *current = *current + range->step;

    if (*current < range->end)
        return (Object*) Integer_fromLongLong(*current);

    return LoxStopIteration;
}

Iterator*
range_iterate_int(Object *self) {
    assert(self->type == &LoxIntRangeType);

    LoxIntRangeIterator* iter = (LoxIntRangeIterator*) LoxIterator_create(self, sizeof(LoxRangeIterator));
    iter->iterator.next = loxrange_next_int;
    iter->current = ((LoxIntRange*) self) ->start;
    return (Iterator*) iter;
}

static struct object_type LoxIntRangeType = (ObjectType) {
    .name = "range",
    .iterate = range_iterate_int,

    //.as_string = range_asstring,
};