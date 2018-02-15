#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "garbage.h"
#include "object.h"

void
garbage_collect(Object* object) {
    assert(object->refcount == 0);
    assert(object->type != NULL);

    if (object->type->cleanup != NULL)
        object->type->cleanup(object);

    free(object);
}
