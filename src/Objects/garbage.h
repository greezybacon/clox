#ifndef GARBAGE_H
#define GARBAGE_H

#include "object.h"

void
garbage_collect(Object* object);

#define INCREF(object) ((Object*) object)->refcount++
#define DECREF(object) do { if (0 == --((Object*) object)->refcount) garbage_collect(object); } while(0)
// TODO: Trigger garbage collection if refcount==0

#endif
