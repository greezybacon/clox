#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "object.h"
#include "Vendor/bdwgc/include/gc.h"

static void
object_finalize(GC_PTR object, GC_PTR client_data) {
    Object* self = (Object*) object;
    assert(self->type != NULL);
    assert(self->type->cleanup != NULL);

    self->type->cleanup(self);
}


void*
object_new(size_t size, ObjectType* type) {
    Object* result = (void*) GC_MALLOC(size);

	if (result == NULL) {
		// TODO: Trigger error
	}
	result->type = type;

    if (type->cleanup)
        GC_REGISTER_FINALIZER(result, object_finalize, NULL, NULL, NULL);

	return (void*) result;
}
