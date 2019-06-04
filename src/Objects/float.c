#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "boolean.h"
#include "float.h"
#include "integer.h"
#include "string.h"

static struct object_type FloatType;

LoxFloat*
Float_fromLongDouble(long double value) {
    LoxFloat* O = object_new(sizeof(LoxFloat), &FloatType);
    O->value = value;
    return O;
}

LoxFloat*
Float_fromLongLong(long long value) {
    LoxFloat* O = object_new(sizeof(LoxFloat), &FloatType);
    O->value = (long double) value;
    return O;
}

bool
Float_isFloat(Object *value) {
    assert(value);
    return value->type == &FloatType;
}

Object*
Float_fromObject(Object* value) {
    assert(value->type);

    if (!value->type->as_int) {
        fprintf(stderr, "Warning: Cannot coerce type `%s` to float\n", value->type->name);
        return LoxUndefined;
    }

    return value->type->as_float(value);
}

long double
Float_toLongDouble(Object* value) {
    assert(value->type);

    if (value->type != &FloatType) {
        if (!value->type->as_float) {
            // TODO: Raise error
            return 0.0;
        }
        value = value->type->as_float(value);
    }

    assert(value->type == &FloatType);

    return ((LoxFloat*) value)->value;
}

hashval_t
float_hash(Object* self) {
    assert(self->type == &FloatType);

    LoxFloat* S = (LoxFloat*) self;
    unsigned long long value = (unsigned long long) S->value;
    return (int) (value >> 32) ^ (value & 0xffffffff);
}

static Object*
float_asint(Object* self) {
    assert(self->type == &FloatType);
    return (Object*) Integer_fromLongLong((long long) ((LoxFloat*) self)->value);
}

static Object*
float_asfloat(Object* self) {
    assert(self->type == &FloatType);
    return (Object*) self;
}

static Object*
float_asstring(Object* self) {
    assert(self->type == &FloatType);

    char buffer[40];
    snprintf(buffer, sizeof(buffer), "%Lg", ((LoxFloat*) self)->value);
    return (Object*) String_fromCharArrayAndSize(buffer, strlen(buffer));
}

static struct object*
float_op_plus(Object* self, Object* other) {
    assert(self->type == &FloatType);

    if (other->type->code != TYPE_FLOAT) {
        if (other->type->as_float == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_float(other);
    }
    return (Object*) Float_fromLongDouble(((LoxFloat*) self)->value + ((LoxFloat*) other)->value);
}

static struct object*
float_op_minus(Object* self, Object* other) {
    assert(self->type == &FloatType);

    if (other->type->code != TYPE_FLOAT) {
        if (other->type->as_float == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_float(other);
    }
    return (Object*) Float_fromLongDouble(((LoxFloat*) self)->value - ((LoxFloat*) other)->value);
}

static struct object*
float_op_neg(Object* self) {
    assert(self->type == &FloatType);

    return (Object*) Float_fromLongDouble(- ((LoxFloat*) self)->value);
}

static struct object*
float_op_star(Object* self, Object* other) {
    assert(self->type->code == TYPE_FLOAT);

    if (other->type->code != TYPE_FLOAT) {
        if (other->type->as_float == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_float(other);
    }
    return (Object*) Float_fromLongDouble(((LoxFloat*) self)->value * ((LoxFloat*) other)->value);
}

static struct object*
float_op_slash(Object* self, Object* other) {
    assert(self->type->code == TYPE_FLOAT);

    if (other->type->code != TYPE_FLOAT) {
        if (other->type->as_float == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_float(other);
    }
    return (Object*) Float_fromLongDouble(((LoxFloat*) self)->value / ((LoxFloat*) other)->value);
}

static inline Object*
coerce_float(Object* value) {
    if (value->type != &FloatType) {
       if (value->type->as_float == NULL) {
           fprintf(stderr, "Warning: Cannot coerce type `%s` to float\n", value->type->name);
           return LoxUndefined;
       }
       value = (Object*) value->type->as_float(value);
   }
   return value;
}

static int
float_compare(Object *self, Object *other) {
    assert(self->type->code == TYPE_FLOAT);

    other = coerce_float(other);

    long double difference = ((LoxFloat*) self)->value - ((LoxFloat*) other)->value;
    return fabsl(difference) < DBL_EPSILON ?
        0 : (difference > 0 ? 1 : -1);
}

static struct object_type FloatType = (ObjectType) {
    .code = TYPE_FLOAT,
    .name = "float",
    .features = FEATURE_STASH,
    .hash = float_hash,
    .as_int = float_asint,
    .as_float = float_asfloat,
    .as_string = float_asstring,

    .op_plus = float_op_plus,
    .op_minus = float_op_minus,
    .op_star = float_op_star,
    .op_slash = float_op_slash,
    .op_neg = float_op_neg,

    .compare = float_compare,
};