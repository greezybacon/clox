#include <stdlib.h>

#include "object.h"

void*
object_new(size_t size, ObjectType* type) {
	Object* result = calloc(1, size);

	if (result == NULL) {
		// TODO: Trigger error
	}

	INCREF(result);
	result->type = type;

	return (void*) result;
}
