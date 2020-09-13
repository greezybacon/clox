#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "object.h"
#include "boolean.h"
#include "integer.h"
#include "list.h"
#include "string.h"

#include "Lib/builtin.h"

#include "Vendor/bdwgc/include/gc.h"

static struct object_type StringType;
static struct object_type StringTreeType;

LoxString*
String_fromCharsAndSize(const char *characters, size_t size) {
    LoxString* O = object_new(sizeof(LoxString), &StringType);
    O->length = size;
    O->characters = strndup(characters, size);
    return O;
}

LoxString*
String_fromMalloc(const char *characters, size_t size) {
    LoxString* O = object_new(sizeof(LoxString), &StringType);
    O->length = size;
    O->characters = characters;
    return O;
}

LoxString*
String_fromObject(Object* value) {
    assert(value->type);

    if (value->type->as_string)
        return (LoxString*) value->type->as_string(value);

    char buffer[32];
    size_t length = snprintf(buffer, sizeof(buffer), "object<%s>@%p", value->type->name, value);
    return String_fromCharsAndSize(buffer, length);
}

LoxString*
String_fromLiteral(const char* value, size_t size) {
    // TODO: Interpret backslash-escaped characters
    return String_fromCharsAndSize(value, size);
}

LoxString*
String_fromConstant(const char* value) {
    return String_fromMalloc(value, strlen(value));
}

Object*
LoxString_Build(int count, ...) {
    va_list strings;
    va_start(strings, count);

    Object *result = NULL;

    while (count--) {
        if (!result)
            result = va_arg(strings, Object*);
        else
            result = result->type->op_plus(result, va_arg(strings, Object*));
    }

    va_end(strings);
    return result;
}

Object*
LoxString_BuildFromList(int count, Object **first) {
    Object *result = NULL, *current;

    while (count--) {
        current = *first++;
        if (!String_isString(current))
            current = LoxObject_Format(current, "");
        if (!result)
            result = current;
        else
            result = result->type->op_plus(result, current);
    }
    return result;
}

static void
string_cleanup(Object *self) {
    assert(self->type == &StringType);

    LoxString* this = (LoxString*) self;
    free((void*) this->characters);
}

static hashval_t
_string_hash_block(Object *self, hashval_t initial) {
    // Hash implementation adapted from http://www.azillionmonkeys.com/qed/hash.html
    if (unlikely(self == NULL))
        return initial;

    LoxString* S = (LoxString*) self;
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

    LoxString* S = (LoxString*) self;

    return _string_hash_final(self, S->length);
}

static Object*
string_len(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringType);

    LoxString* S = (LoxString*) self;
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

static LoxBool*
string_asbool(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringType);

    return ((LoxString*) self)->length == 0 ? LoxFALSE : LoxTRUE;
}

static Object*
string_asint(Object* self) {
    LoxString* string = (LoxString*) self;
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

    int cmp = strncmp(((LoxString*) self)->characters,
        ((LoxString*) other)->characters,
        ((LoxString*) self)->length);

    return cmp != 0 ? cmp
        : ((LoxString*) self)->length - ((LoxString*) other)->length;
}

static Object*
string_op_plus(Object* self, Object* other) {
    assert(self != NULL);
    assert(self->type == &StringType);

    if (other->type == &StringTreeType)
        return (Object*) StringTree_fromStrings(self, other);
    if (!String_isString(other))
        other = other->type->as_string(other);

    // If adding something like single chars, build it as a string. Plus,
    // keeping the length as multiples of 4 characters, where possible, would
    // mean for more consistent hashing between string trees and normal
    // strings for the same sequence of characters.
    if (((LoxString*)self)->length < 64) {
        char *buffer;
        asprintf(&buffer, "%.*s%.*s", ((LoxString*)self)->length, ((LoxString*)self)->characters,
            ((LoxString*)other)->length, ((LoxString*)other)->characters);
        return (Object*) String_fromMalloc(buffer, ((LoxString*)self)->length + ((LoxString*)other)->length);
    }

    // Otherwise, try not to duplicate memory
    return (Object*) StringTree_fromStrings(self, other);
}

const char*
LoxString_getCharAt(LoxString *self, int index, int *length) {
    assert(self != NULL);

    LoxString *S = (LoxString*) self;
    int j = 0;

    while (index < 0)
        index += S->length;

    const char *s = S->characters;
    if (index > 0) {
        // Advance to the start of the character following the one we want. Then
        // back up one byte afterwards.
        index++;
        while (index) {
            if ((*s++ & 0xc0) != 0x80)
                index--;
        }
        s--;
    }
    else {
        // Go backwards, looking for the first char in the sequence. This
        // would be much better than scanning the entire string to find the
        // last char.
        s += S->length - 1;
        while (index < 0) {
            if ((*s++ & 0xc0) != 0x80)
                index++;
        }
        s++;
    }

    // So, `s` points at the unicode character requested. But we should
    // include all the bytes in the response.
    *length = 1;
    const char *t = s;
    while (((*++t & 0xc0) == 0x80))
        (*length)++;

    return s;
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
    LoxString *S = (LoxString*) self;
    if (S->length <= i)
        // TODO: Raise exception
        return LoxNIL;

    int length;
    const char *s = LoxString_getCharAt(S, i, &length);

    // TODO: Maybe this could be a StringSlice object?
    // XXX: Possible memory corruption if this string is cleaned up while a
    // reference to the slice is in use.
    return (Object*) String_fromMalloc(s, length);
}

static Object*
string_chars__next(Iterator *self) {
    LoxStringIterator *this = (LoxStringIterator*) self;
    LoxString* target = (LoxString*) self->target;

    if (this->pos < target->length) {
        const char *s = target->characters + this->pos;
        // So, `s` points at the unicode character requested. But we should
        // include all the bytes in the response.
        unsigned length = 1;
        const char *t = s;
        while (((*++t & 0xc0) == 0x80))
            length++;

        this->pos += length;
        // XXX: Handle case where length unexpectedly goes beyond the end of
        // the string?

        // TODO: Maybe this could be a StringSlice object?
        // XXX: Possible memory corruption if this string is cleaned up while a
        // reference to the slice is in use.
        return (Object*) String_fromMalloc(s, length);
    }

    return LoxStopIteration;
}

static Iterator*
string_iterate(Object *self) {
    LoxStringIterator* it = (LoxStringIterator*) LoxIterator_create((Object*) self, sizeof(LoxStringIterator));
    it->iterator.next = string_chars__next;
    return (Iterator*) it;
}

// METHODS ----------------------------------

Object*
string_upper(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &StringType);

    LoxString *S = (LoxString*) self;
    char *upper = malloc(S->length), *eupper = upper;
    int i;
    for (i=0; i<S->length; i++)
        *eupper++ = toupper(upper[i]);

    return (Object*) String_fromMalloc(upper, S->length);
}

Object*
string_rtrim(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &StringType);

    Object *chars = NULL;
    Lox_ParseArgs(args, "|O", &chars);

    if (chars == NULL) {
        chars = (Object*) String_fromConstant(" \r\n\t");
    }
    INCREF(chars);

    // TODO: Coerce `chars` to LoxString

    LoxList *list = LoxList_new();
    INCREF(list);
    LoxList_extend(list, chars);
    int char_count = LoxList_getLength(list);

    LoxString *S = (LoxString*) self;
    const char *s1;
    Object *s2;
    int i, l1, l2, length = S->length, pos = -1;
    bool stop;
    do {
        s1 = LoxString_getCharAt(S, pos, &l1);
        i = char_count;
        stop = true;
        while (i--) {
            s2 = LoxList_getItem(list, i);
            assert(s2->type == &StringType);
            if (String_compare((LoxString*) s2, s1) == 0) {
                length--;
                pos--;
                stop = false;
                break;
            }
        }
    }
    while (!stop);

    DECREF(list);
    DECREF(chars);

    if (S->length == length)
        return self;

    return (Object*) String_fromCharsAndSize(S->characters, length);
}

static struct object_type StringType = (ObjectType) {
    .code = TYPE_STRING,
    .name = "string",
    .hash = string_hash,
    .len = string_len,

    .as_int = string_asint,
    .as_string = string_asstring,
    .as_bool = string_asbool,

    .compare = string_compare,
    .iterate = string_iterate,

    .op_plus = string_op_plus,

    .get_item = string_getitem,

    .properties = (ObjectProperty[]) {
        {"upper", string_upper},
        {"rtrim", string_rtrim},
        {0, 0},
    },
};

static LoxString _LoxEmptyString = (LoxString) {
    .base = (Object) {
        .type = &StringType,
        .refcount = 1,
    },
    .length = 0,
    .characters = "",
};
const LoxString *LoxEmptyString = &_LoxEmptyString;

bool
String_isString(Object* value) {
    if (value->type && value->type == &StringType)
        return true;

    return false;
}

int
String_compare(LoxString* left, const char* right) {
    assert(left);
    assert(right);
    assert(left->base.type == &StringType);

    return strncmp(left->characters, right, left->length);
}


LoxStringTree*
StringTree_fromStrings(Object *a, Object *b) {
    LoxStringTree* O = object_new(sizeof(LoxStringTree), &StringTreeType);

    O->left = (Object*) a;
    O->right = (Object*) b;
    INCREF(a);
    INCREF(b);

    return O;
}

bool
StringTree_isStringTree(Object *O) {
    return O->type == &StringTreeType;
}

static void
stringtree_cleanup(Object* self) {
    assert(self->type == &StringTreeType);

    LoxStringTree* this = (LoxStringTree*) self;
    DECREF(this->left);
    DECREF(this->right);
}

static Object*
stringtree_len(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    LoxStringTree* S = (LoxStringTree*) self;

    int total = Integer_toInt(S->left->type->len(S->left));
    return (Object*) Integer_fromLongLong(total + Integer_toInt(S->right->type->len(S->right)));
}

static hashval_t
stringtree_hash(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    Object *chunk;
    Iterator *chunks = LoxStringTree_iterChunks((LoxStringTree*) self);
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
            ((LoxString*) object)->length, ((LoxString*) object)->characters);
    }
    else if (object->type == &StringTreeType) {
        size_t length;
        length = stringtree_copy_buffer(((LoxStringTree*) object)->left, buffer, size);
        buffer += length;
        size -= length;
        return length + stringtree_copy_buffer(((LoxStringTree*) object)->right, buffer, size);
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


static LoxBool*
stringtree_asbool(Object* self) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);
    return Integer_toInt(stringtree_len(self)) > 0 ? LoxTRUE : LoxFALSE;
}

Object*
stringtree_chunks__next(Iterator* this) {
    ChunkIterator *self = (ChunkIterator*) this;

    // TODO: Support type assertion
    LoxStringTree *target = (LoxStringTree*) self->iterator.target;

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
        if (target->left->type == &StringTreeType) {
            self->inner = LoxStringTree_iterChunks(target->left);
            INCREF(self->inner);
            return stringtree_chunks__next(self->inner);
        }
        else {
            return target->left;
        }
    }
    else if (self->pos == 1) {
        self->pos++;
        if (target->right->type == &StringTreeType) {
            self->inner = LoxStringTree_iterChunks(target->right);
            INCREF(self->inner);
            return stringtree_chunks__next(self->inner);
        }
        else {
            return target->right;
        }
    }

    // Signal end of iteration
    return LoxStopIteration;
}

Iterator*
LoxStringTree_iterChunks(LoxStringTree* self) {
    ChunkIterator* it = (ChunkIterator*) LoxIterator_create((Object*) self, sizeof(ChunkIterator));

    it->iterator.next = stringtree_chunks__next;

    return (Iterator*) it;
}

Object*
stringtree_chunks(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &StringTreeType);

    return (Object*) LoxStringTree_iterChunks((LoxStringTree*) self);
}

/*
static int
stringtree_compare(Object *self, Object *other) {
    assert(self != NULL);
    assert(self->type == &StringTreeType);

    LoxStringTree* S = (LoxStringTree*) self;

    // TODO: Compare the string to the other. Return TRUE if it compares to 0
}
*/

static Object*
stringtree_op_plus(Object *self, Object *other) {
    LoxStringTree *this = (LoxStringTree*) self;
    Object *old = this->right;

    this->right = this->right->type->op_plus(this->right, other);
    INCREF(this->right);
    DECREF(old);

    return self;
}

static struct object_type StringTreeType = (ObjectType) {
    .code = TYPE_STRINGTREE,
    .name = "string(tree)",
    .cleanup = stringtree_cleanup,
    .hash = stringtree_hash,
    .len = stringtree_len,

    .as_string = stringtree_asstring,
    .as_bool = stringtree_asbool,

    .op_plus = stringtree_op_plus,

    .properties = (ObjectProperty[]) {
        { "chunks", stringtree_chunks },
        { 0, 0 },
    },
};
