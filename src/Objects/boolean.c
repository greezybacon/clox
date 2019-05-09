#include <assert.h>

#include "object.h"
#include "boolean.h"

#include "integer.h"
#include "string.h"

#include "Eval/error.h"

static struct object_type BooleanType;

BoolObject*
Bool_fromBool(bool value) {
    return value ? LoxTRUE : LoxFALSE;
}

BoolObject*
Bool_fromObject(Object* value) {
    if (value->type == &BooleanType)
        return (BoolObject*) value;
    else if (value->type->as_bool)
        return value->type->as_bool(value);
    // TODO: If type has a len() method, compare > zero
    else if (value == LoxNIL)
        return LoxFALSE;
    else
        // XXX: FIXME
        eval_error(NULL, "Cannot represent value as Boolean");
}

bool
Bool_isBool(Object* value) {
    if (!value)
        return false;

    return value->type == &BooleanType;
}

bool
Bool_isTrue(Object* value) {
    if (value->type != &BooleanType)
        value = (Object*) Bool_fromObject(value);
    return value == (Object*) LoxTRUE;
}

static BoolObject*
bool_self(Object* self) {
    assert(self->type == &BooleanType);
    return (BoolObject*) self;
}

static Object*
bool_asint(Object* self) {
    assert(self->type == &BooleanType);
    return (Object*) Integer_fromLongLong(((BoolObject*)self)->value ? 1 : 0);
}

static Object*
bool_asstring(Object* self) {
    assert(self->type == &BooleanType);
    return (Object*) String_fromConstant(((BoolObject*)self)->value ? "true" : "false");
}

static BoolObject*
bool_op_eq(Object* self, Object* other) {
    assert(self->type == &BooleanType);
    if (other->type != self->type) {
        if (!other->type->as_bool) {
            // Raise error
        }
        other = (Object*) other->type->as_bool(other);
    }
    return (((BoolObject*)self)->value == ((BoolObject*)other)->value
        ? LoxTRUE : LoxFALSE);
}

static BoolObject*
bool_op_ne(Object* self, Object* other) {
    return (bool_op_eq(self, other) == LoxTRUE) ? LoxFALSE : LoxTRUE;
}

static struct object_type BooleanType = (ObjectType) {
    .code = TYPE_BOOL,
    .name = "bool",

    .as_bool = bool_self,
    .as_int = bool_asint,
    .as_string = bool_asstring,

    .op_eq = bool_op_eq,
    .op_ne = bool_op_ne,

    .cleanup = ERROR,
};

static BoolObject _LoxTRUE = (BoolObject) {
    .base.type = &BooleanType,
    .base.refcount = 1,
    .value = true,
};
BoolObject *LoxTRUE = &_LoxTRUE;

static BoolObject _LoxFALSE = (BoolObject) {
    .base.type = &BooleanType,
    .base.refcount = 1,
    .value = false,
};
BoolObject *LoxFALSE = &_LoxFALSE;




static BoolObject*
null_asbool(Object* self) {
    return LoxFALSE;
}

static Object*
null_asint(Object* self) {
    return (Object*) Integer_fromLongLong(0);
}

static Object*
null_asstring(Object* self) {
    return (Object*) String_fromConstant("null");
}

static struct object_type NilType = (ObjectType) {
    .code = TYPE_NIL,
    .name = "nil",

    .as_bool = null_asbool,
    .as_int = null_asint,
    .as_string = null_asstring,

    .op_eq = IDENTITY,
    .cleanup = ERROR,
};

static Object _LoxNIL = (Object) {
    .type = &NilType,
    .refcount = 1,
};
Object *LoxNIL = &_LoxNIL;




static BoolObject*
undef_asbool(Object* self) {
    return LoxFALSE;
}

static Object*
undef_asstring(Object* self) {
    return (Object*) String_fromConstant("undefned");
}

static struct object_type UndefinedType = (ObjectType) {
    .code = TYPE_UNDEFINED,
    .name = "undefined",

    .as_bool = undef_asbool,
    .as_string = undef_asstring,

    .op_eq = IDENTITY,
    .cleanup = ERROR,
};

static Object _LoxUndefined = (Object) {
    .type = &UndefinedType,
    .refcount = 1,
};
Object *LoxUndefined = &_LoxUndefined;