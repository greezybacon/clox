#include <assert.h>

#include "object.h"
#include "boolean.h"

#include "integer.h"
#include "string.h"

static struct object_type BooleanType;

Object*
Bool_fromBool(bool value) {
    return (Object*) (value ? LoxTRUE : LoxFALSE);
}

static Object*
bool_self(Object* self) {
    return self;
}

static Object*
bool_asint(Object* self) {
    assert(self->type == &BooleanType);
    return (Object*) Integer_fromLongLong(((BoolObject*)self)->value ? 1 : 0);
}

static Object*
bool_asstring(Object* self) {
    assert(self->type == &BooleanType);
    return (Object*) String_fromCharArrayAndSize(((BoolObject*)self)->value ? "true:" : "false", 5);
}

static Object*
bool_op_eq(Object* self, Object* other) {
    assert(self->type == &BooleanType);
    if (other->type != self->type) {
        if (!other->type->as_bool) {
            // Raise error
        }
        other = other->type->as_bool(other);
    }
    return (Object*) (((BoolObject*)self)->value == ((BoolObject*)other)->value
        ? LoxTRUE : LoxFALSE);
}

static Object*
bool_op_ne(Object* self, Object* other) {
    return bool_op_eq(self, other) == LoxTRUE ? LoxFALSE : LoxTRUE;
}

static struct object_type BooleanType = (ObjectType) {
    .code = TYPE_BOOL,
    .name = "bool",

    .as_bool = bool_self,
    .as_int = bool_asint,
    .as_string = bool_asstring,
};

static BoolObject _LoxTRUE = (BoolObject) {
    .base.type = &BooleanType,
    .value = true,
};
BoolObject *LoxTRUE = &_LoxTRUE;

static BoolObject _LoxFALSE = (BoolObject) {
    .base.type = &BooleanType,
    .value = false,
};
BoolObject *LoxFALSE = &_LoxFALSE;