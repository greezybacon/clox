#ifndef OBJECT_H
#define OBJECT_H

#include <stdbool.h>
#include <stdlib.h>

enum base_type {
    TYPE_UNDEFINED=0,
    TYPE_NIL,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_STRINGTREE,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_HASH,
    TYPE_FUNCTION,
    TYPE_CLASS,
    TYPE_OBJECT,
    TYPE_BOUND_METHOD,
    TYPE_EXCEPTION,
    TYPE_ITERATOR,
};

enum object_type_feature {
    FEATURE_STASH =     1<<0,
};

typedef struct object Object;
typedef long long int hashval_t;
typedef struct bool_object LoxBool;
typedef struct vmeval_scope VmScope;
typedef struct hash_object LoxTable;

typedef struct lox_iterator Iterator;

typedef Object* (*NativeFunctionCall)(VmScope*, Object*, Object*);

enum object_property_type {
    OBJECT_PROP_TYPE_METHOD=0,
    OBJECT_PROP_TYPE_CONSTANT,
    OBJECT_PROP_TYPE_PROPERTY,
};

typedef struct object_property {
    const char*            name;
    union {
        NativeFunctionCall method;
        Object             *value;
        struct {
            NativeFunctionCall getter;
            NativeFunctionCall setter;
            NativeFunctionCall delete;
        }                   property;
    };
    enum object_property_type type;
} ObjectProperty;

typedef struct object_type {
    enum base_type  code;
    char*           name;
    enum object_type_feature features;

    // Hashtable support
    hashval_t (*hash)(Object*);

    // Coercion to simple type
    Object* (*as_int)(Object*);
    Object* (*as_float)(Object*);
    LoxBool* (*as_bool)(Object*);
    Object* (*as_string)(Object*);
    Object* (*format)(Object*, Object*);

    // Item access
    Object* (*get_item)(Object*, Object*);
    void (*set_item)(Object*, Object*, Object*);
    void (*remove_item)(Object*, Object*);
    LoxBool* (*contains)(Object*, Object*);
    Object* (*len)(Object*);

    // TODO: Binary operators (plus, etc.)
    Object* (*op_plus)(Object*, Object*);
    Object* (*op_minus)(Object*, Object*);
    Object* (*op_star)(Object*, Object*);
    Object* (*op_pow)(Object*, Object*);
    Object* (*op_slash)(Object*, Object*);
    Object* (*op_mod)(Object*, Object*);
    Object* (*op_lshift)(Object*, Object*);
    Object* (*op_rshift)(Object*, Object*);
    Object* (*op_band)(Object*, Object*);
    Object* (*op_bor)(Object*, Object*);
    Object* (*op_xor)(Object*, Object*);

    // TODO: Unary operators
    Object* (*op_neg)(Object*);

    // TODO: Comparison
    int (*compare)(Object*, Object*);

    // TODO: Callable
	Object* (*call)(Object*, VmScope*, Object*, Object*);

    // Iteration
    Iterator* (*iterate)(Object*);

    // TODO: Methods (stuff unique to each type)
    ObjectProperty* properties;
    // XXX: Move this to an opaque type? Something behind-the-scenes-ish
    // XXX: Change to `properties` and include getter methods?
    LoxTable* _methodTable;
    Object* (*getattr)(Object*, Object*, hashval_t);
    void (*setattr)(Object*, Object*, Object*, hashval_t);

    // TODO: Garbage collection cooperation
    void (*cleanup)(Object*);
} ObjectType;

typedef struct object {
    ObjectType* type;
    bool protect_delete;
    unsigned refcount;
} Object;

void* object_new(size_t size, ObjectType*);
Object* object_getattr(Object*, Object*, hashval_t);
void LoxObject_Cleanup(Object*);

#define INCREF(object) (((Object*) object)->refcount++)
#define DECREF(object) do { if ((--((Object*) object)->refcount) == 0) LoxObject_Cleanup((Object*) object); } while(0)

#define HASHVAL(object) (hashval_t) ((object)->type->hash \
    ? (object)->type->hash(object) \
    : (hashval_t) (object))

static hashval_t MYADDRESS(Object *self) {
    Object **pself = &self;
    return (hashval_t) *pself;
}

#include "boolean.h"
struct bool_object *LoxTRUE;
struct bool_object *LoxFALSE;
static inline int IDENTITY(Object *self, Object *other) {
    return self == other ? 0 : -1;
}

static inline void ERROR(Object *self) {
    *((volatile int*) ((volatile void*) NULL)) = 42;
}

#endif
