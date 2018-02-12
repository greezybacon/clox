#ifndef OBJECT_H
#define OBJECT_H

#include <stdlib.h>

enum base_type {
    TYPE_NIL=1,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_HASH,
    TYPE_FUNCTION,
};

typedef struct object Object;
typedef unsigned long int hashval_t;
typedef struct bool_object BoolObject;
typedef struct interp_context Interpreter;

typedef struct object_method {
    char*       name;
    Object*     (*method)(Interpreter*, Object*, Object*);
} ObjectMethod;

typedef struct object_type {
    enum base_type  code;
    char*           name;
    int             features;

    // Hashtable support
    hashval_t (*hash)(Object*);

    // Coercion
    Object* (*as_int)(Object*);
    Object* (*as_float)(Object*);
    BoolObject* (*as_bool)(Object*);
    Object* (*as_string)(Object*);

    // Item access
    Object* (*get_item)(Object*, Object*);
    void (*set_item)(Object*, Object*, Object*);
    void (*remove_item)(Object*, Object*);
    BoolObject* (*contains)(Object*, Object*);
    Object* (*len)(Object*);

    // TODO: Binary operators (plus, etc.)
    Object* (*op_plus)(Object*, Object*);
    Object* (*op_minus)(Object*, Object*);
    Object* (*op_star)(Object*, Object*);
    Object* (*op_slash)(Object*, Object*);

    // TODO: Unary operators
    Object* (*op_neg)(Object*);

    // TODO: Comparison
    BoolObject* (*op_lt)(Object*, Object*);
    BoolObject* (*op_lte)(Object*, Object*);
    BoolObject* (*op_gt)(Object*, Object*);
    BoolObject* (*op_gte)(Object*, Object*);
    BoolObject* (*op_eq)(Object*, Object*);
    BoolObject* (*op_ne)(Object*, Object*);
    Object* (*compare)(Object*, Object*);

    // TODO: Callable
	Object* (*call)(Object*, Interpreter*, Object*, Object*);

    // TODO: Methods (stuff unique to each type)
    ObjectMethod* methods;

    // TODO: Garbage collection cooperation
    void (*cleanup)(Object*);
} ObjectType;

typedef struct object {
    ObjectType* type;
    unsigned    refcount;
} Object;

void*
object_new(size_t size, ObjectType*);

#endif
