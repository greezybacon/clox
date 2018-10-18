#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "object.h"
#include "function.h"
#include "string.h"
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

Object*
object_getattr(Object* self, Object *name) {
    assert(self);

    if (self->type->getattr)
        return self->type->getattr(self, name);

    if (self->type->methods) {
        // XXX: This is terribly slow -- move to a automatically-created hashtable?
        ObjectMethod* method = self->type->methods;
        while (method) {
            if (String_compare(name, method->name) == 0)
                return NativeFunction_bind(NativeFunction_new(method->method), self);
            method++;
        }
    }

    return LoxNIL;
}