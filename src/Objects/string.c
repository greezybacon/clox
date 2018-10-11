#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "object.h"
#include "boolean.h"
#include "integer.h"
#include "string.h"

#include "Vendor/bdwgc/include/gc.h"

static struct object_type StringType;
static struct object_type StringTreeType;

StringObject*
String_fromCharArrayAndSize(char *characters, size_t size) {
    StringObject* O = object_new(sizeof(StringObject), &StringType);
    O->length = size;
    O->characters = GC_STRNDUP(characters, size);
    return O;
}

static StringObject*
String_fromMalloc(char *characters, size_t size) {
    StringObject* O = object_new(sizeof(StringObject), &StringType);
    O->length = size;
    O->characters = characters;
    return O;
}

StringObject*
String_fromObject(Object* value) {
    assert(value->type);

    if (value->type->as_string)
        return (StringObject*) value->type->as_string(value);

    char buffer[32];
    size_t length = snprintf(buffer, sizeof(buffer), "object@%p", value);
    return String_fromCharArrayAndSize(buffer, length);
}

StringObject*
String_fromLiteral(char* value, size_t size) {
    // TODO: Interpret backslash-escaped characters
    return String_fromCharArrayAndSize(value + 1, size - 2);
}

static hashval_t
string_hash(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringType);

    StringObject* S = (StringObject*) self;
    char *ch = S->characters;
    unsigned length = S->length;
    hashval_t hash = 0;

    while (length--)
        hash = (hash << 4) + hash + *ch++;

    // TODO: Consider caching the hash value
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

static BoolObject*
string_asbool(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringType);

    return ((StringObject*) self)->length == 0 ? LoxFALSE : LoxTRUE;
}

static Object*
string_asint(Object* self) {
    StringObject* string = (StringObject*) self;
    // TODO: (asint) should support base (10 or 16)
    char* endpos;
    long long value = strtoll(string->characters, &endpos, 10);

    // TODO: Check for invalid numbers
    // TODO: endpos should be at the end of the string
    return (Object*) Integer_fromLongLong(value);
}

static BoolObject*
string_op_eq(Object* self, Object* other) {
    assert(self->type == &StringType);
    assert(other);
    
    if (other->type != self->type)
        return LoxFALSE;

    if (self == other)
        return LoxTRUE;

    return strncmp(((StringObject*) self)->characters, ((StringObject*) other)->characters,
        ((StringObject*) self)->length) == 0
        ? LoxTRUE : LoxFALSE;
}

static BoolObject*
string_op_ne(Object* self, Object* other) {
    return string_op_eq(self, other) == LoxTRUE ? LoxFALSE : LoxTRUE;
}

static Object*
string_op_plus(Object* self, Object* other) {
    assert(self != NULL);
    assert(self->type == &StringType);

    if (other->type == &StringTreeType)
        return StringTree_fromStrings(self, other);
    if (!String_isString(other))
        other = other->type->as_string(other);

    return (Object*) StringTree_fromStrings((StringObject*) self, other);
}

static Object*
string_getitem(Object* self, Object* index) {
    assert(self != NULL);
    assert(self->type == &StringType);

    
}

static struct object_type StringType = (ObjectType) {
    .code = TYPE_STRING,
    .name = "string",
    .hash = string_hash,
    .len = string_len,

    .as_int = string_asint,
    .as_string = string_asstring,
    .as_bool = string_asbool,

    .op_eq = string_op_eq,
    .op_ne = string_op_ne,

    .op_plus = string_op_plus,
};

bool
String_isString(Object* value) {
    if (value->type && value->type == &StringType)
        return true;

    return false;
}



StringTreeObject*
StringTree_fromStrings(Object *a, Object *b) {
    StringTreeObject* O = object_new(sizeof(StringTreeObject), &StringTreeType);

    O->left = (Object*) a;
    O->right = (Object*) b;

    return O;
}

static hashval_t
stringtree_hash(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    StringTreeObject* S = (StringTreeObject*) self;

    hashval_t left = S->left->type->hash(S->left);
    return (left << 4) + left + S->right->type->hash(S->right);
}

static Object*
stringtree_len(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    StringTreeObject* S = (StringTreeObject*) self;

    int total = Integer_toInt(S->left->type->len(S->left));
    return (Object*) Integer_fromLongLong(total + Integer_toInt(S->right->type->len(S->right)));
}

static int
stringtree_copy_buffer(Object* object, char *buffer, size_t size) {
    if (object->type == &StringType) {
        return snprintf(buffer, size, "%.*s",
            ((StringObject*) object)->length, ((StringObject*) object)->characters);
    }
    else if (object->type == &StringTreeType) {
        size_t length;
        length = stringtree_copy_buffer(((StringTreeObject*) object)->left, buffer, size);
        buffer += length;
        size -= length;
        return length + stringtree_copy_buffer(((StringTreeObject*) object)->right, buffer, size);
    }

    return 0;
}


static Object*
stringtree_asstring(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    size_t size = Integer_toInt(stringtree_len(self)) + 1;

    char *buffer = GC_MALLOC_ATOMIC(sizeof(char) * size), *pbuffer = buffer;
    if (!buffer) {
        return LoxNIL;
    }

    // XXX: Get some performance testing on this idea
    size = stringtree_copy_buffer(self, buffer, size);

    return (Object*) String_fromMalloc(buffer, size);
}


static BoolObject*
stringtree_asbool(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);
    return Integer_toInt(stringtree_len(self)) > 0 ? LoxTRUE : LoxFALSE;
}

static BoolObject*
stringtree_op_eq(Object *self, Object *other) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    StringTreeObject* S = (StringTreeObject*) self;

    // TODO: Compare the string to the other. Return TRUE if it compares to 0
}

static Object*
stringtree_op_plus(Object *self, Object *other) {
    StringTreeObject* O = object_new(sizeof(StringTreeObject), &StringTreeType);

    if (!String_isString(other))
        other = other->type->as_string(other);

    O->left = self;
    O->right = other;

    return (Object*) O;
}



static struct object_type StringTreeType = (ObjectType) {
    .code = TYPE_STRINGTREE,
    .name = "string",
    .hash = stringtree_hash,
    .len = stringtree_len,

    .as_string = stringtree_asstring,
    .as_bool = stringtree_asbool,

    .op_plus = stringtree_op_plus,
};
