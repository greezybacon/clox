#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "object.h"
#include "boolean.h"
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

    IntegerObject* I = value->type->as_int(value);
    return I->value;
}

static int
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

    snprintf(buffer, sizeof(buffer), "%lld", ((IntegerObject*) self)->value);
    return (Object*) String_fromCharArrayAndSize(buffer, sizeof(buffer));
}

static struct object*
integer_op_plus(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type->code == TYPE_FLOAT) {
        return other->type->op_plus(other, self);
    }
    // Else coerce to integer
    else if (other->type != &IntegerType) {
        if (other->type->as_int == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_int(other);
    }
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
    else if (other->type != &IntegerType) {
        if (other->type->as_int == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_int(other);
    }
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
    else if (other->type->code != TYPE_INTEGER) {
        if (other->type->as_int == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_int(other);
    }
    return (Object*) Integer_fromLongLong(((IntegerObject*) self)->value * ((IntegerObject*) other)->value);
}

static struct object*
integer_op_divide(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    // Promote floating point operations
    if (other->type->code == TYPE_FLOAT) {
        return other->type->op_slash(other, self);
    }
    // Else coerce to integer
    else if (other->type->code != TYPE_INTEGER) {
        if (other->type->as_int == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_int(other);
    }
    return (Object*) Integer_fromLongLong(((IntegerObject*) self)->value / ((IntegerObject*) other)->value);
}

static Object*
integer_op_eq(Object* self, Object* other) {
    assert(self->type == &IntegerType);

    if (other->type->code != TYPE_INTEGER) {
        if (other->type->as_int == NULL) {
            // interpreter_raise('Cannot add (object) to int')
        }
        other = other->type->as_int(other);
    }
    return ((IntegerObject*) self)->value == ((IntegerObject*) other)->value ? LoxTRUE : LoxFALSE;
}

static Object*
integer_op_ne(Object* self, Object* other) {
    return integer_op_eq(self, other) == LoxTRUE ? LoxFALSE : LoxTRUE;;
}

static struct object_type IntegerType = (ObjectType) {
    .code = TYPE_INTEGER,
    .name = "int",
    .hash = integer_hash,
    
    // coercion
    .as_int = integer_asint,
    .as_float = integer_asfloat,
    .as_string = integer_asstring,

    // math operations
    .op_plus = integer_op_plus,
	.op_minus = integer_op_minus,
	.op_star = integer_op_multiply,
	.op_slash = integer_op_divide,

    .op_eq = integer_op_eq,
    .op_ne = integer_op_ne,
};
