#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "float.h"
#include "integer.h"
#include "string.h"

static struct object_type FloatType;

FloatObject*
Float_fromLongDouble(long double value) {
    FloatObject* O = object_new(sizeof(FloatObject), &FloatType);
    O->value = value;
    return O;
}

FloatObject*
Float_fromLongLong(long long value) {
    FloatObject* O = object_new(sizeof(FloatObject), &FloatType);
    O->value = (long double) value;
    return O;
}

static int
float_hash(Object* self) {
    assert(self->type->code == TYPE_FLOAT);

    FloatObject* S = (FloatObject*) self;
    unsigned long long value = (unsigned long long) S->value;
    return (int) (value >> 32) ^ (value & 0xffffffff);
}

static Object*
float_asint(Object* self) {
    assert(self->type->code == TYPE_FLOAT);
    return (Object*) Integer_fromLongLong((long long) ((FloatObject*) self)->value);
}

static Object*
float_asfloat(Object* self) {
    assert(self->type->code == TYPE_FLOAT);
    return (Object*) self;
}

static Object*
float_asstring(Object* self) {
    assert(self->type->code == TYPE_FLOAT);

    char buffer[40];
    snprintf(buffer, sizeof(buffer), "%Lg", ((FloatObject*) self)->value);
    return (Object*) String_fromCharArrayAndSize(buffer, strlen(buffer));
}

static struct object*
float_op_plus(Object* self, Object* other) {
    assert(self->type->code == TYPE_FLOAT);

    if (other->type->code != TYPE_FLOAT) {
        if (other->type->as_float == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_float(other);
    }
    return (Object*) Float_fromLongDouble(((FloatObject*) self)->value + ((FloatObject*) other)->value);
}

static struct object*
float_op_minus(Object* self, Object* other) {
    assert(self->type->code == TYPE_FLOAT);

    if (other->type->code != TYPE_FLOAT) {
        if (other->type->as_float == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_float(other);
    }
    return (Object*) Float_fromLongDouble(((FloatObject*) self)->value - ((FloatObject*) other)->value);
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
    return (Object*) Float_fromLongDouble(((FloatObject*) self)->value * ((FloatObject*) other)->value);
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
    return (Object*) Float_fromLongDouble(((FloatObject*) self)->value / ((FloatObject*) other)->value);
}

static struct object_type FloatType = (ObjectType) {
    .code = TYPE_FLOAT,
    .name = "float",
    .hash = float_hash,
    .as_int = float_asint,
    .as_float = float_asfloat,
    .as_string = float_asstring,

    .op_plus = float_op_plus,
    .op_minus = float_op_minus,
    .op_star = float_op_star,
    .op_slash = float_op_slash,
};