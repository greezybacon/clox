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

typedef struct object_method {
    struct object* name;
    struct object* (*callable)(struct object*);
} ObjectMethod;

typedef struct object_type {
    enum base_type  code;
    char*           name;
    int             features;

    // Hashtable support
    unsigned long int (*hash)(Object*);

    // Coercion
    Object* (*as_int)(Object*);
    Object* (*as_float)(Object*);
    Object* (*as_bool)(Object*);
    Object* (*as_string)(Object*);

    // Item access
    Object* (*get_item)(Object*, Object*);
    void (*set_item)(Object*, Object*, Object*);
    void (*remove_item)(Object*, Object*);
    Object* (*contains)(Object*, Object*);
    Object* (*len)(Object*);

    // TODO: Math operators (plus, etc.)
    Object* (*op_plus)(Object*, Object*);
    Object* (*op_minus)(Object*, Object*);
    Object* (*op_star)(Object*, Object*);
    Object* (*op_slash)(Object*, Object*);
    
    // TODO: Comparison
    Object* (*op_lt)(Object*, Object*);
    Object* (*op_lte)(Object*, Object*);
    Object* (*op_gt)(Object*, Object*);
    Object* (*op_gte)(Object*, Object*);
    Object* (*op_eq)(Object*, Object*);
    Object* (*op_ne)(Object*, Object*);
    Object* (*compare)(Object*, Object*);

    // TODO: Callable
	Object* (*call)(Object*, void*);

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
