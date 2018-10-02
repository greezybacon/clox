#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "garbage.h"
#include "object.h"

static GarbageHeap *pile = NULL;

static void
_garbage_collect(Object* object) {
    assert(object->refcount == 0);
    assert(object->type != NULL);

    if (object->type->cleanup != NULL)
        object->type->cleanup(object);

    free(object);
}

void
garbage_collect(void) {
    printf("Collecting garbage...\n");
    PieceOfTrash *leftover = pile->heap, *item = pile->heap;
    int i;
    for (i=0; i<GARBAGE_HEAP_SIZE; i++) {
        if (pile->count == 0)
            break;
        if (item->inuse && item->object->refcount == 0) {
            _garbage_collect(item->object);
            pile->count--;
        }
        // Either way, evict from garbage.
        item->inuse = false;
        item++; // = item->next;
    }
}

static PieceOfTrash *
_garbage_find_slot(void) {
    int i;
    PieceOfTrash *item = pile->heap;
    for (i=0; i<GARBAGE_HEAP_SIZE; i++) {
        if (!item->inuse)
            return item;
        item++;
    }
    return NULL;
}

void
garbage_mark(Object* object) {
    if (pile == NULL)
        pile = calloc(1, sizeof(GarbageHeap));

    PieceOfTrash *item = _garbage_find_slot();
    if (!item) {
        garbage_collect();
        garbage_mark(object);
    }
    else {
        item->object = object;
        item->inuse = true;
        pile->count++;
    }
}