#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "Lib/builtin.h"
#include "list.h"
#include "integer.h"
#include "iterator.h"
#include "string.h"

const int BUCKET_SIZE = 16;
const int HALF_BUCKET_SIZE = BUCKET_SIZE >> 1;

static struct object_type ListType;

LoxList*
LoxList_new(void) {
    LoxList* self = object_new(sizeof(LoxList), &ListType);

    ListBucket *bucket = malloc(sizeof(ListBucket));
    *bucket = (ListBucket) {
        .size = BUCKET_SIZE,
        .items = calloc(BUCKET_SIZE, sizeof(Object*)),
    };

    self->buckets = bucket;
    return self;
}

void
LoxList_append(LoxList *self, Object *item) {
    // Fast-forward to list end
    ListBucket *end = self->buckets;
    while (end->next)
        end = end->next;

    if (end->count == end->size) {
        // Add a new bucket (split if inserting)
        ListBucket *bucket = malloc(sizeof(ListBucket));
        *bucket = (ListBucket) {
            .size = BUCKET_SIZE,
            .offset = end->offset + end->count,
            .items = calloc(BUCKET_SIZE, sizeof(Object*)),
        };
        end->next = bucket;
        end = bucket;
    }

    INCREF(item);
    *(end->items + end->count) = item;

    end->count++;
    self->count++;
}

void
LoxList_extend(LoxList *self, Object *object) {
    if (!object->type->iterate) {
        fprintf(stderr, "WARNING: Item to extend list must be iterable.\n");
        return;
    }

    Iterator *items = object->type->iterate(object);
    Object *item;
    while (LoxStopIteration != (item = items->next(items))) {
        LoxList_append(self, item);
    }
    LoxObject_Cleanup((Object*) items);
}

Object*
LoxList_pop(LoxList *self) {
    // Fast-forward to list end
    ListBucket *end = self->buckets, *previous = NULL;
    while (end->next) {
        previous = end;
        end = end->next;
    }

    Object *result = *(end->items + end->count - 1);
    if (--end->count == 0 && previous) {
        previous->next = NULL;
        free(end->items);
        free(end);
    }

    self->count--;
    DECREF(result);
    return result;
}

Object*
LoxList_popAt(LoxList *self, int index) {
    while (index < 0)
        index += self->count;

    if (index >= self->count) {
        fprintf(stderr, "Warning: List pop index is after list end\n");
        return LoxUndefined;
    }

    // Fast-forward to bucket containing index
    ListBucket *end = self->buckets, *previous = NULL;
    while (end->offset < index) {
        previous = end;
        end = end->next;
    }

    Object *result = *(end->items + index - end->offset);
    if (--end->count == 0 && previous) {
        previous->next = end->next;
        free(end->items);
        free(end);
    }
    else {
        // TODO: If the list bucket is greater than BUCKET_SIZE, then just split
        // the list somewhere after item being popped resides and decrement the
        // starting offset of the following bucket.

        // Rewrite the `items` list. The items after the index element should
        // be moved up in the items list
        while (index < end->count) {
            *(end->items + index) = *(end->items + index + 1);
            index++;
        }

        // Decrement the offset of all other buckets
        while (end->next) {
            end = end->next;
            end->offset--;
        }
    }

    self->count--;
    DECREF(result);
    return result;
}

Object*
LoxList_getItem(LoxList *self, int index) {
    while (index < 0)
        index += self->count;

    if (index >= self->count) {
        fprintf(stderr, "Warning: List item index is after list end\n");
        return LoxUndefined;
    }

    // Fast-forward to bucket containing index
    ListBucket *end = self->buckets;
    while (end && index >= (end->offset + end->count)) {
        end = end->next;
    }

    if (!end)
        return LoxUndefined;

    return *(end->items + index - end->offset);
}

void
LoxList_setItem(LoxList *self, int index, Object* item) {
    while (index < 0)
        index += self->count;

    if (index > self->count) {
        fprintf(stderr, "Warning: List item index is after list end\n");
    }

    // Fast-forward to bucket containing index
    ListBucket *end = self->buckets;
    while (end->offset < index) {
        end = end->next;
    }

    *(end->items + index - end->offset) = item;
}

static Object*
list_entries__next(Iterator *self) {
    LoxListIterator *this = (LoxListIterator*) self;

    if (this->pos_in_bucket >= this->bucket->count) {
        this->bucket = this->bucket->next;
        this->pos_in_bucket = 0;
    }

    if (this->bucket && this->pos_in_bucket < this->bucket->count)
        return *(this->bucket->items + this->pos_in_bucket++);

    return LoxStopIteration;
}

Iterator*
LoxList_getIterator(LoxList* self) {
    LoxListIterator* it = (LoxListIterator*) LoxIterator_create((Object*) self, sizeof(LoxListIterator));

    it->iterator.next = list_entries__next;
    it->bucket = self->buckets;

    return (Iterator*) it;
}

static void
list_cleanup(Object *self) {
    assert(self->type == &ListType);
    LoxList *this = (LoxList*) self;

    ListBucket *end = this->buckets;
    while (end) {
        while (end->count--)
            DECREF(end->items + end->count);
        free(end->items);
        free(end);
        // XXX: Use after free?
        end = end->next;
    }
}

static Object*
list_len(Object *self) {
    assert(self->type == &ListType);
    LoxList *this = (LoxList*) self;

    return (Object*) Integer_fromLongLong(this->count);
}

static Object*
list_getitem(Object *self, Object *index) {
    assert(self->type == &ListType);
    LoxList *this = (LoxList*) self;

    if (!Integer_isInteger(index))
        index = Integer_fromObject(index);

    return LoxList_getItem(this, Integer_toInt(index));
}

static void
list_setitem(Object *self, Object *index, Object *value) {
    assert(self->type == &ListType);
    LoxList *this = (LoxList*) self;

    if (!Integer_isInteger(index))
        index = Integer_fromObject(index);

    return LoxList_setItem(this, Integer_toInt(index), value);
}

static Object*
list_append(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &ListType);

    Object *object;
    Lox_ParseArgs(args, "O", &object);

    LoxList_append((LoxList*) self, object);
    return LoxNIL;
}

static Object*
list_extend(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &ListType);

    Object *object;
    Lox_ParseArgs(args, "O", &object);

    LoxList_extend((LoxList*) self, object);
    return LoxNIL;
}

static Object*
list_pop(VmScope *state, Object *self, Object *args) {
    assert(self->type == &ListType);

    if (Tuple_getSize(args) == 0)
        return LoxList_pop((LoxList*) self);

    int i;
    Lox_ParseArgs(args, "i", &i);

    return LoxList_popAt((LoxList*) self, i);
}

static Iterator*
list_iterate(Object *self) {
    assert(self);
    assert(self->type == &ListType);

    return LoxList_getIterator((LoxList*) self);
}

static Object*
list_asstring(Object* self) {
    assert(self->type == &ListType);

    char buffer[1024];  // TODO: Use the + operator of LoxString
    char* position = buffer;
    int remaining = sizeof(buffer) - 1, bytes;

    bytes = snprintf(position, remaining, "[");
    position += bytes, remaining -= bytes;

    Object *value;
    LoxString *svalue;
    LoxList *this = (LoxList*) self;
    Iterator *it = LoxList_getIterator(this);
    int k = this->count, i = 0;

    while (LoxStopIteration != (value = it->next(it))) {
        svalue = String_fromObject(value);
        INCREF(svalue);
        bytes = snprintf(position, remaining, "%.*s%s",
            svalue->length, svalue->characters,
            ++i == k ? "" : ", ");
        position += bytes, remaining -= bytes;
        DECREF(svalue);
    }
    LoxObject_Cleanup((Object*) it);

    position += snprintf(position, remaining, "]");

    return (Object*) String_fromCharArrayAndSize(buffer, position - buffer);
}

static int object_type_compare(const void *left, const void *right) {
    Object *lhs = *(Object**) left, *rhs = *(Object**) right;
    return lhs->type->compare(lhs, rhs);
}

static Object*
list_sort(VmScope *state, Object *self, Object *args) {
    assert(self->type == &ListType);

    // If the list fits in one bucket, then just sort that bucket.
    // Otherwise, allocate a new bucket big enough for all the buckets and move
    // all the items inside. Then sort the one bucket.
    LoxList *this = (LoxList*) self;
    if (this->buckets->next) {
        ListBucket *bucket, *pbucket, *newbucket = malloc(sizeof(ListBucket));
        *newbucket = (ListBucket) {
            .size = this->count,
            .offset = 0,
            .items = malloc(this->count * sizeof(Object*)),
        };

        // Copy the items and release the buckets
        Object **olditems, **newitems = newbucket->items;
        int count;
        while (bucket) {
            olditems = bucket->items;
            count = bucket->count;
            while (count--) {
                *(newitems++) = *(olditems++);
            }
            free(bucket->items);
            pbucket = bucket->next;
            free(bucket);
            bucket = pbucket;
        }

        this->buckets = newbucket;
    }

    qsort(this->buckets->items, this->count, sizeof(Object*), object_type_compare);
    return LoxNIL;
}


static struct object_type ListType = (ObjectType) {
    .name = "list",
    .len = list_len,
    .get_item = list_getitem,
    .set_item = list_setitem,
    .iterate = list_iterate,

    .as_string = list_asstring,
    .cleanup = list_cleanup,

    .methods = (ObjectMethod[]) {
        { "append", list_append },
        { "extend", list_extend },
        { "sort",   list_sort },
        { "pop",    list_pop },
        { 0, 0 },
    },
};
