#include <assert.h>
#include <ctype.h>
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
    O->characters = strndup(characters, size);
    return O;
}

StringObject*
String_fromMalloc(const char *characters, size_t size) {
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
    size_t length = snprintf(buffer, sizeof(buffer), "object<%s>@%p", value->type->name, value);
    return String_fromCharArrayAndSize(buffer, length);
}

StringObject*
String_fromLiteral(char* value, size_t size) {
    // TODO: Interpret backslash-escaped characters
    return String_fromCharArrayAndSize(value + 1, size - 2);
}

StringObject*
String_fromConstant(const char* value) {
    return String_fromMalloc(value, strlen(value));
}

static void
string_cleanup(Object *self) {
    assert(self->type == &StringType);

    StringObject* this = (StringObject*) self;
    free((void*) this->characters);
}

static hashval_t
_string_hash_block(Object *self, hashval_t initial) {
    // Hash implementation adapted from http://www.azillionmonkeys.com/qed/hash.html
    if (unlikely(self == NULL))
        return initial;

    StringObject* S = (StringObject*) self;
    const char *data = S->characters;
    unsigned length = S->length;
    hashval_t hash = initial, tmp;
    int rem;

    if (length <= 0)
        return initial;

    rem = length & 3;
    length >>= 2;

    /* Main loop */
    for (; length > 0; length--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    return hash;
}

static hashval_t
_string_hash_final(Object *self, hashval_t initial) {
    hashval_t hash = _string_hash_block(self, initial);

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    // TODO: Consider caching the hash value
    return hash;
}

static hashval_t
string_hash(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringType);

    StringObject* S = (StringObject*) self;

    return _string_hash_final(self, S->length);
}

static Object*
string_len(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringType);

    StringObject* S = (StringObject*) self;
    if (S->length > 0 && S->char_count == 0) {
		// Faster counting, http://www.daemonology.net/blog/2008-06-05-faster-utf8-strlen.html
   		int i = S->length, j = 0;
		const char *s = S->characters;
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

static int
string_compare(Object *self, Object *other) {
    assert(self->type == &StringType);
    assert(other->type == &StringType);

    int cmp = strncmp(((StringObject*) self)->characters,
        ((StringObject*) other)->characters,
        ((StringObject*) self)->length);

        return cmp != 0 ? cmp
            : ((StringObject*) self)->length - ((StringObject*) other)->length;
}

static BoolObject*
string_op_eq(Object* self, Object* other) {
    assert(self->type == &StringType);
    assert(other);

    if (other->type != self->type)
        return LoxFALSE;

    if (self == other)
        return LoxTRUE;

    return string_compare(self, other) == 0 ? LoxTRUE : LoxFALSE;
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
        return (Object*) StringTree_fromStrings(self, other);
    if (!String_isString(other))
        other = other->type->as_string(other);

    return (Object*) StringTree_fromStrings(self, other);
}

static Object*
string_getitem(Object* self, Object* index) {
    assert(self != NULL);
    assert(self->type == &StringType);

    // This will be the same algorithm as the len method; however, it will
    // scan to the appropriate, requested spot
    if (!Integer_isInteger(index)) {
        if (!index->type->as_int) {
            // TODO: Raise exception
            return LoxNIL;
        }
        index = index->type->as_int(index);
    }

    int i = Integer_toInt(index);
    StringObject *S = (StringObject*) self;
    int length = S->length, j = 0;

    if (length <= i)
        // TODO: Raise exception
        return LoxNIL;

    while (i < 0)
        i += S->length;

    // Advance to the start of the character following the one we want. Then
    // back up one byte afterwards.
    i++;
    const char *s = S->characters;
    while (i) {
        if ((*s++ & 0xc0) != 0x80)
            i--;
	}
    s--;

    // So, `s` points at the unicode character requested. But we should
    // include all the bytes in the response.
    length = 1;
    const char *t = s;
    while (((*++t & 0xc0) == 0x80))
        length++;

    // TODO: Maybe this could be a StringSlice object?
    // XXX: Possible memory corruption if this string is cleaned up while a
    // reference to the slice is in use.
    return (Object*) String_fromMalloc(s, length);
}

// METHODS ----------------------------------

Object*
string_upper(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &StringType);

    StringObject *S = (StringObject*) self;
    char *upper = strndup(S->characters, S->length), *eupper = upper;
    int i;
    for (i=0; i<S->length; i++)
        *eupper++ = toupper(upper[i]);

    return (Object*) String_fromMalloc(upper, S->length);
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

    .get_item = string_getitem,

    .methods = (ObjectMethod[]) {
        {"upper", string_upper},
        {0, 0},
    },
};

static StringObject _LoxEmptyString = (StringObject) {
    .base = (Object) {
        .type = &StringType,
        .refcount = 1,
    },
    .length = 0,
    .characters = "",
};
const StringObject *LoxEmptyString = &_LoxEmptyString;

bool
String_isString(Object* value) {
    if (value->type && value->type == &StringType)
        return true;

    return false;
}

int
String_compare(StringObject* left, const char* right) {
    assert(left);
    assert(right);
    assert(left->base.type == &StringType);

    return strncmp(left->characters, right, left->length);
}


StringTreeObject*
StringTree_fromStrings(Object *a, Object *b) {
    StringTreeObject* O = object_new(sizeof(StringTreeObject), &StringTreeType);

    O->left = (Object*) a;
    O->right = (Object*) b;
    INCREF(a);
    INCREF(b);

    return O;
}

static void
stringtree_cleanup(Object* self) {
    assert(self->type == &StringTreeType);

    StringTreeObject* this = (StringTreeObject*) self;
    DECREF(this->left);
    DECREF(this->right);
}

static Object*
stringtree_len(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    StringTreeObject* S = (StringTreeObject*) self;

    int total = Integer_toInt(S->left->type->len(S->left));
    return (Object*) Integer_fromLongLong(total + Integer_toInt(S->right->type->len(S->right)));
}

static hashval_t
stringtree_hash(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    Object *chunk;
    Iterator *chunks = LoxStringTree_iterChunks((StringTreeObject*) self);
    hashval_t value = Integer_toInt(stringtree_len(self));

    while (LoxStopIteration != (chunk = chunks->next(chunks))) {
        // XXX: To calculate the same value as the equivalent contiguous
        // string, the characters should be copied to represent and even
        // number of 4-byte blocks with the remainder only at the end.
        value = _string_hash_block(chunk, value);
    }
    LoxObject_Cleanup((Object*) chunks);

    return _string_hash_final(NULL, value);
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

    char *buffer = malloc(sizeof(char) * size), *pbuffer = buffer;
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

Object*
stringtree_chunks__next(ChunkIterator* self) {
    // TODO: Support type assertion

    if (self->inner) {
        Object *result = stringtree_chunks__next(self->inner);
        if (result != LoxStopIteration)
            return result;

        // Else retire inner iterator and move ahead
        DECREF(self->inner);
        self->inner = NULL;
    }

    if (self->pos == 0) {
        self->pos++;
        if (self->tree->left->type == &StringTreeType) {
            self->inner = (ChunkIterator*) LoxStringTree_iterChunks(self->tree->left);
            INCREF(self->inner);
            return stringtree_chunks__next(self->inner);
        }
        else {
            return (StringObject*) self->tree->left;
        }
    }
    else if (self->pos == 1) {
        self->pos++;
        if (self->tree->right->type == &StringTreeType) {
            self->inner = (ChunkIterator*) LoxStringTree_iterChunks(self->tree->right);
            INCREF(self->inner);
            return stringtree_chunks__next(self->inner);
        }
        else {
            return (StringObject*) self->tree->right;
        }
    }

    // Signal end of iteration
    return LoxStopIteration;
}

Iterator*
LoxStringTree_iterChunks(StringTreeObject* self) {
    ChunkIterator* it = LoxIterator_create(sizeof(ChunkIterator));

    it->iterator.next = stringtree_chunks__next;
    it->tree = self;
    INCREF((Object*) self);

    return (Iterator*) it;
}

Object*
stringtree_chunks(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &StringTreeType);

    return (Object*) LoxStringTree_iterChunks((StringTreeObject*) self);
}

/*
static int
stringtree_compare(Object *self, Object *other) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    StringTreeObject* S = (StringTreeObject*) self;

    // TODO: Compare the string to the other. Return TRUE if it compares to 0
}

static Object*
stringtree_op_plus(Object *self, Object *other) {
    StringTreeObject* O = object_new(sizeof(StringTreeObject), &StringTreeType);

    if (other->type != &StringTreeType && !String_isString(other))
        other = other->type->as_string(other);

    O->left = self;
    O->right = other;
    INCREF(self);
    INCREF(other);

    return (Object*) O;
}

static struct object_type StringTreeType = (ObjectType) {
    .code = TYPE_STRINGTREE,
    .name = "string",
    .cleanup = stringtree_cleanup,
    .hash = stringtree_hash,
    .len = stringtree_len,

    .as_string = stringtree_asstring,
    .as_bool = stringtree_asbool,

    .op_plus = stringtree_op_plus,

    .methods = (ObjectMethod[]) {
        { "chunks", stringtree_chunks },
        { 0, 0 },
    },
};
