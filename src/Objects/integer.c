#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "boolean.h"
#include "object.h"
#include "integer.h"
#include "float.h"
#include "string.h"

static struct object_type IntegerType;

#define SMALL_INTS 16
static LoxInteger SmallIntegers[SMALL_INTS] = {
    [0] = (LoxInteger) { .value = 0, .base.protect_delete = true, .base.type = &IntegerType},
    [1] = (LoxInteger) { .value = 1, .base.protect_delete = true, .base.type = &IntegerType },
    [2] = (LoxInteger) { .value = 2, .base.protect_delete = true, .base.type = &IntegerType },
    [3] = (LoxInteger) { .value = 3, .base.protect_delete = true, .base.type = &IntegerType },
    [4] = (LoxInteger) { .value = 4, .base.protect_delete = true, .base.type = &IntegerType },
    [5] = (LoxInteger) { .value = 5, .base.protect_delete = true, .base.type = &IntegerType },
    [6] = (LoxInteger) { .value = 6, .base.protect_delete = true, .base.type = &IntegerType },
    [7] = (LoxInteger) { .value = 7, .base.protect_delete = true, .base.type = &IntegerType },
    [8] = (LoxInteger) { .value = 8, .base.protect_delete = true, .base.type = &IntegerType },
    [9] = (LoxInteger) { .value = 9, .base.protect_delete = true, .base.type = &IntegerType },
    [10] = (LoxInteger) { .value = 10, .base.protect_delete = true, .base.type = &IntegerType },
    [11] = (LoxInteger) { .value = 11, .base.protect_delete = true, .base.type = &IntegerType },
    [12] = (LoxInteger) { .value = 12, .base.protect_delete = true, .base.type = &IntegerType },
    [13] = (LoxInteger) { .value = 13, .base.protect_delete = true, .base.type = &IntegerType },
    [14] = (LoxInteger) { .value = 14, .base.protect_delete = true, .base.type = &IntegerType },
    [15] = (LoxInteger) { .value = 15, .base.protect_delete = true, .base.type = &IntegerType },
};

LoxInteger*
Integer_fromLongLong(long long value) {
    if (value < SMALL_INTS && value > 0) {
        return &SmallIntegers[value];
    }

    LoxInteger* O = object_new(sizeof(LoxInteger), &IntegerType);
    O->value = value;
    return O;
}

long long
Integer_toInt(Object* value) {
    assert(value->type);

    if (value->type != &IntegerType) {
        if (!value->type->as_int) {
            // TODO: Raise error
        }
        value = value->type->as_int(value);
    }

    assert(value->type == &IntegerType);

    return ((LoxInteger*) value)->value;
}

bool
Integer_isInteger(Object *value) {
    assert(value);
    return value->type == &IntegerType;
}

Object*
Integer_fromObject(Object* value) {
    assert(value->type);

    if (!value->type->as_int) {
        fprintf(stderr, "Warning: Cannot coerce type `%s` to int\n", value->type->name);
        return LoxUndefined;
    }

    return value->type->as_int(value);
}

static hashval_t
integer_hash(Object* self) {
    assert(self != NULL);
    assert(self->type == &IntegerType);

    LoxInteger* S = (LoxInteger*) self;
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

    return (Object*) Float_fromLongLong(((LoxInteger*) self)->value);
}

static Object*
integer_asstring(Object* self) {
    assert(self != NULL);
    assert(self->type == &IntegerType);

    char buffer[32];
    int length;

    length = snprintf(buffer, sizeof(buffer), "%lld", ((LoxInteger*) self)->value);
    return (Object*) String_fromCharsAndSize(buffer, length);
}

static LoxBool*
integer_asbool(Object* self) {
    assert(self != NULL);
    assert(self->type == &IntegerType);

    return ((LoxInteger*) self)->value == 0 ? LoxFALSE : LoxTRUE;
}

static inline Object*
coerce_integer(Object* value) {
    if (value->type != &IntegerType) {
       if (value->type->as_int == NULL) {
           fprintf(stderr, "Warning: Cannot coerce type `%s` to float\n", value->type->name);
           return LoxUndefined;
       }
       value = value->type->as_int(value);
   }
   return value;
}

static struct object*
integer_op_plus(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type != &IntegerType) {
        if (other->type->code == TYPE_FLOAT) {
            return other->type->op_plus(other, self);
        }
        // Else coerce to integer
        else other = coerce_integer(other);
    }

    return (Object*) Integer_fromLongLong(((LoxInteger*) self)->value + ((LoxInteger*) other)->value);
}

static struct object*
integer_op_minus(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type != &IntegerType) {
        if (other->type->code == TYPE_FLOAT) {
            Object *F = self->type->as_float(self);
            return F->type->op_minus(F, other);
        }
        // Else coerce to integer
        else other = coerce_integer(other);
    }

    return (Object*) Integer_fromLongLong(((LoxInteger*) self)->value - ((LoxInteger*) other)->value);
}

static struct object*
integer_op_mod(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    other = coerce_integer(other);
    return (Object*) Integer_fromLongLong(((LoxInteger*) self)->value % ((LoxInteger*) other)->value);
}

static struct object*
integer_op_lshift(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    other = coerce_integer(other);
    return (Object*) Integer_fromLongLong(((LoxInteger*) self)->value << ((LoxInteger*) other)->value);
}

static struct object*
integer_op_rshift(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    other = coerce_integer(other);
    return (Object*) Integer_fromLongLong(((LoxInteger*) self)->value >> ((LoxInteger*) other)->value);
}

static struct object*
integer_op_band(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    other = coerce_integer(other);
    return (Object*) Integer_fromLongLong(((LoxInteger*) self)->value & ((LoxInteger*) other)->value);
}

static struct object*
integer_op_bor(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    other = coerce_integer(other);
    return (Object*) Integer_fromLongLong(((LoxInteger*) self)->value | ((LoxInteger*) other)->value);
}

static struct object*
integer_op_neg(Object* self) {
    assert(self->type == &IntegerType);

    return (Object*) Integer_fromLongLong(- ((LoxInteger*) self)->value);
}

static struct object*
integer_op_multiply(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type != &IntegerType) {
        if (other->type->code == TYPE_FLOAT) {
            return other->type->op_star(other, self);
        }
        // Else coerce to integer
        else other = coerce_integer(other);
    }
    return (Object*) Integer_fromLongLong(((LoxInteger*) self)->value * ((LoxInteger*) other)->value);
}

static struct object*
integer_op_divide(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type != &IntegerType) {
        if (other->type->code == TYPE_FLOAT) {
            LoxFloat *F = (LoxFloat*) self->type->as_float(self);
            return F->base.type->op_slash((Object*) F, other);
        }
        // Else coerce to integer
        else other = coerce_integer(other);
    }
    return (Object*) Integer_fromLongLong(((LoxInteger*) self)->value / ((LoxInteger*) other)->value);
}

static int
integer_compare(Object *self, Object *other) {
    if (!(other = coerce_integer(other)))
        return -1;

    return ((LoxInteger*) self)->value - ((LoxInteger*) other)->value;
}


static struct object_type IntegerType = (ObjectType) {
    .code = TYPE_INTEGER,
    .name = "int",
    .features = FEATURE_STASH,
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
    .op_mod = integer_op_mod,
    .op_lshift = integer_op_lshift,
    .op_rshift = integer_op_rshift,
    .op_bor = integer_op_bor,
    .op_band = integer_op_band,
    .op_neg = integer_op_neg,

    // comparison
    .compare = integer_compare,
};
