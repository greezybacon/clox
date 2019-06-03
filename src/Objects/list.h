#ifndef LIST_H
#define LIST_H

#include <stdbool.h>

#include "object.h"
#include "iterator.h"

typedef struct list_bucket {
    int                 offset;
    int                 size;
    int                 count;
    Object**            items;
    struct list_bucket  *next;
} ListBucket;

typedef struct list_object {
    // Inherits from Object
    Object      base;

    int         count;
    ListBucket* buckets;
} LoxList;

typedef struct {
    union {
        Object      object;
        Iterator    iterator;
    };
    int         pos_in_bucket;
    ListBucket  *bucket;
} LoxListIterator;

LoxList* LoxList_new(void);
LoxList* LoxList_fromArgs(size_t, ...);
LoxList* LoxList_fromList(size_t, Object**);
Object* LoxList_getItem(LoxList*, int);
void LoxList_setItem(LoxList*, int, Object*);
void LoxList_append(LoxList*, Object*);
void LoxList_extend(LoxList*, Object*);
Object* LoxList_pop(LoxList*);
Object* LoxList_popAt(LoxList*, int);

bool LoxList_isList(Object*);
size_t LoxList_getSize(Object*);

#define LoxList_GETITEM(self, index) *(((LoxList*) self)->items + index)
#endif
