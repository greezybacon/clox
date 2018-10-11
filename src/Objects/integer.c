#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "boolean.h"
#include "object.h"
#include "integer.h"
#include "float.h"
#include "string.h"

static struct object_type IntegerType;

IntegerObject*
Integer_fromLongLong(long long value) {
    IntegerObject* O = object_new(sizeof(IntegerObject), &IntegerType);
    O->value = value;
    return O;
}

long long
Integer_toInt(Object* value) {
    assert(value->type);
    if (!value->type->as_int) {
        // TODO: Raise error
    }

    IntegerObject* I = (IntegerObject*) value->type->as_int(value);
    assert(I->base.type == &IntegerType);

    return I->value;
}

static hashval_t
integer_hash(Object* self) {
    assert(self != NULL);
    assert(self->type == &IntegerType);

    IntegerObject* S = (IntegerObject*) self;
    return (int) (S->value >> 32) ^ (S->value & 0xffffffff);
}

static struct object*
integer_asint(Object* self) {
    return self;
}

static struct object*
integer_asfloat(Object* self) {
    assert(self != NULL);
    assert(self->type == &IntegerType);

    return (Object*) Float_fromLongLong(((IntegerObject*) self)->value);
}

static Object*
integer_asstring(Object* self) {
    assert(self != NULL);
    assert(self->type == &IntegerType);

    char buffer[32];
    int length;

    length = snprintf(buffer, sizeof(buffer), "%lld", ((IntegerObject*) self)->value);
    return (Object*) String_fromCharArrayAndSize(buffer, length);
}

static BoolObject*
integer_asbool(Object* self) {
    assert(self != NULL);
    assert(self->type == &IntegerType);

    return ((IntegerObject*) self)->value == 0 ? LoxFALSE : LoxTRUE;
}

static inline Object*
coerce_integer(Object* value) {
    if (value->type != &IntegerType) {
       if (value->type->as_int == NULL) {
           eval_error("Value cannot be coerced to int");
           return NULL;
       }
       value = value->type->as_int(value);
   }
   return value;
}

static struct object*
integer_op_plus(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type->code == TYPE_FLOAT) {
        return other->type->op_plus(other, self);
    }
    // Else coerce to integer
    else other = coerce_integer(other);

    return (Object*) Integer_fromLongLong(((IntegerObject*) self)->value + ((IntegerObject*) other)->value);
}

static struct object*
integer_op_minus(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type->code == TYPE_FLOAT) {
        return other->type->op_minus(other, self);
    }
    // Else coerce to integer
    else other = coerce_integer(other);
    return (Object*) Integer_fromLongLong(((IntegerObject*) self)->value - ((IntegerObject*) other)->value);
}

static struct object*
integer_op_multiply(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type->code == TYPE_FLOAT) {
        return other->type->op_star(other, self);
    }
    // Else coerce to integer
    else other = coerce_integer(other);
    return (Object*) Integer_fromLongLong(((IntegerObject*) self)->value * ((IntegerObject*) other)->value);
}

static struct object*
integer_op_divide(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type->code == TYPE_FLOAT) {
        FloatObject *F = self->type->as_float(self),
                    *rv = F->base.type->op_slash((Object*) F, other);
        return rv;
    }
    // Else coerce to integer
    else other = coerce_integer(other);
    return (Object*) Integer_fromLongLong(((IntegerObject*) self)->value / ((IntegerObject*) other)->value);
}

static BoolObject*
integer_op_eq(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    if (!(other = coerce_integer(other)))
        return LoxFALSE;

    return ((IntegerObject*) self)->value == ((IntegerObject*) other)->value ? LoxTRUE : LoxFALSE;
}

static BoolObject*
integer_op_ne(Object* self, Object* other) {
    return integer_op_eq(self, other) == LoxTRUE ? LoxFALSE : LoxTRUE;
}

static BoolObject*
integer_op_lt(Object* self, Object* other) {
    assert(self->type == &IntegerType);
    IntegerObject* rhs = coerce_integer(other);
    return ((IntegerObject*)self)->value < rhs->value
        ? LoxTRUE : LoxFALSE;
}

static BoolObject*
integer_op_lte(Object* self, Object* other) {
    assert(self->type == &IntegerType);
    IntegerObject* rhs = coerce_integer(other);
    return ((IntegerObject*)self)->value <= rhs->value
        ? LoxTRUE : LoxFALSE;
}

static BoolObject*
integer_op_gt(Object* self, Object* other) {
    assert(self->type == &IntegerType);
    IntegerObject* rhs = coerce_integer(other);
    return ((IntegerObject*)self)->value > rhs->value
        ? LoxTRUE : LoxFALSE;
}

static BoolObject*
integer_op_gte(Object* self, Object* other) {
    assert(self->type == &IntegerType);
    IntegerObject* rhs = coerce_integer(other);
    return ((IntegerObject*)self)->value >= rhs->value
        ? LoxTRUE : LoxFALSE;
}

static struct object_type IntegerType = (ObjectType) {
    .code = TYPE_INTEGER,
    .name = "int",
    .hash = integer_hash,
    
    // coercion
    .as_int = integer_asint,
    .as_float = integer_asfloat,
    .as_string = integer_asstring,
    .as_bool = integer_asbool,

    // math operations
    .op_plus = integer_op_plus,
	.op_minus = integer_op_minus,
	.op_star = integer_op_multiply,
	.op_slash = integer_op_divide,

    // comparison
    .op_lt = integer_op_lt,
    .op_lte = integer_op_lte,
    .op_gt = integer_op_gt,
    .op_gte = integer_op_gte,

    .op_eq = integer_op_eq,
    .op_ne = integer_op_ne,
};
