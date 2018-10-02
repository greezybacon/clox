#ifndef GARBAGE_H
#define GARBAGE_H

#include <stdbool.h>

#include "object.h"

void garbage_collect(void);
void garbage_mark(Object* object);

#define INCREF(object) ((Object*) (object))->refcount++
#define DECREF(object) do { if (0 == --(((Object*) (object))->refcount)) garbage_mark((Object*) (object)); } while(0)
// TODO: Trigger garbage collection if refcount==0

typedef struct possibly_garbage {
    Object                  *object;
    bool                    inuse;
    struct possibly_garbage *next;
} PieceOfTrash;

#define GARBAGE_HEAP_SIZE 32
typedef struct garbage_heap {
    PieceOfTrash            heap[GARBAGE_HEAP_SIZE];
    unsigned                count;
} GarbageHeap;

#endif
