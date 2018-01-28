#include <assert.h>
#include <string.h>

#include "object.h"
#include "boolean.h"
#include "integer.h"
#include "string.h"

static struct object_type StringType;

StringObject*
String_fromCharArrayAndSize(char *characters, size_t size) {
    StringObject* O = object_new(sizeof(StringObject), &StringType);
    O->length = size;
    O->characters = strndup(characters, size);
    return O;
}

static unsigned long int
string_hash(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringType);

    StringObject* S = (StringObject*) self;
    char *ch = S->characters;
    int length = S->length,
        hash = 0;

    while (length--)
        hash = (hash << 4) + hash + *ch++;

    return hash;
}

static Object*
string_len(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringType);

    StringObject* S = (StringObject*) self;
    if (S->length > 0 && S->char_count == 0) {
		// Faster counting, http://www.daemonology.net/blog/2008-06-05-faster-utf8-strlen.html
   		int i = S->length, j = 0;
		char *s = S->characters;
   		while (i--) {
     		if ((*s++ & 0xc0) != 0x80)
				j++;
   		}
		S->char_count = j;
	}
    return (Object*) Integer_fromLongLong(S->char_count);
}

static Object*
string_asstring(Object* self) {
    return self;
}

static Object*
string_op_eq(Object* self, Object* other) {
    assert(self->type == &StringType);
    
    if (other->type != self->type)
        return false;

    return strncmp(((StringObject*) self)->characters, ((StringObject*) other)->characters,
        ((StringObject*) self)->length) == 0
        ? LoxTRUE : LoxFALSE;
}

static Object*
string_op_ne(Object* self, Object* other) {
    return string_op_eq(self, other) == LoxTRUE ? LoxFALSE : LoxTRUE;;
}

static void
string_cleanup(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringType);

    free(((StringObject*) self)->characters);
}

static struct object_type StringType = (ObjectType) {
    .code = TYPE_STRING,
    .name = "string",
    .hash = string_hash,
    .len = string_len,
    
    .as_string = string_asstring,

    .op_eq = string_op_eq,
    .op_ne = string_op_ne,

    .cleanup = string_cleanup,
};

bool
String_isString(Object* value) {
    if (value->type && value->type == &StringType)
        return true;

    return false;
}
