#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "object.h"
#include "class.h"
#include "function.h"
#include "hash.h"
#include "iterator.h"
#include "string.h"

#include "Vendor/bdwgc/include/gc.h"

static struct object_type ClassType;
static struct object_type InstanceType;
static struct object_type BoundMethodType;

ClassObject*
Class_build(HashObject *attributes, ClassObject *parent) {
    ClassObject* O = object_new(sizeof(ClassObject), &ClassType);

    O->attributes = attributes;
    O->parent = parent;
    O->name = LoxUndefined;

    // Attach all the callable attributes to this class
    Iterator* it = Hash_getIterator(attributes);
    Object *value, *next;
    while ((next = it->next(it))) {
        assert(Tuple_isTuple(next));
        value = Tuple_GETITEM((TupleObject*) next, 1);

        if (CodeObject_isCodeObject(value)) {
            ((CodeObject*) value)->context->owner = O;
        }
    }

    return O;
}

bool
Class_isClass(Object* self) {
    assert(self);
    return(self->type == &ClassType);
}

static Object*
class_getattr(Object *self, Object *name) {
    assert(self);
    assert(self->type == &ClassType);

    ClassObject *class = (ClassObject*) self;
    Object *method;
    if (class->attributes && (method = Hash_getItem(class->attributes, name)))
        return method;

    if (class->parent) {
        return class_getattr((Object*) class->parent, name);
    }

    return LoxUndefined;
}

static void
class_setattr(Object *self, Object *name, Object *value) {
    assert(self);
    assert(self->type == &ClassType);

    ClassObject *this = (ClassObject*) self;

    if (!this->attributes)
        this->attributes = Hash_new();

    Hash_setItem(this->attributes, name, value);
}

static Object*
class_instanciate(Object* self, VmScope *scope, Object* object, Object* args) {
    assert(self);
    assert(self->type == &ClassType);

    // Create/return a object with (self) as the class
    InstanceObject *O = object_new(sizeof(InstanceObject), &InstanceType);

    O->class = (ClassObject*) self;

    // Call constructor with args
    static Object *init = NULL;
    if (!init)
        init = (Object*) String_fromCharArrayAndSize("init", 4);

    Object *constructor = class_getattr(self, init);
    if (constructor != LoxUndefined) {
        assert(Function_isCallable(constructor));
        constructor->type->call(constructor, scope, (Object*) O, args);
    }

    return (Object*) O;
}

static Object*
class_asstring(Object *self) {
    char buffer[64];
    size_t bytes;

    bytes = snprintf(buffer, sizeof(buffer), "class@%p", self);
    return (Object*) String_fromCharArrayAndSize(buffer, bytes);
}

static struct object_type ClassType = (ObjectType) {
    .code = TYPE_CLASS,
    .name = "class",
    .hash = MYADDRESS,

    .as_string = class_asstring,

    .op_eq = IDENTITY,
    .getattr = class_getattr,
    .setattr = class_setattr,

    .call = class_instanciate,
};

static Object*
instance_getattr(Object *self, Object *name) {
    assert(self);
    assert(self->type == &InstanceType);

    InstanceObject *this = (InstanceObject*) self;
    assert(this->class);

    Object *attr;

    if (this->attributes && (attr = Hash_getItem(this->attributes, name)))
        return attr;

    if ((attr = class_getattr(this->class, name)) != LoxNIL)
        return BoundMethod_create(attr, self);

    // OOPS. Log something!
    return LoxUndefined;
}

static void
instance_setattr(Object *self, Object *name, Object *value) {
    assert(self);
    assert(self->type == &InstanceType);

    InstanceObject *this = (InstanceObject*) self;

    if (!this->attributes)
        this->attributes = Hash_new();

    Hash_setItem(this->attributes, name, value);
}

static Object*
instance_asstring(Object *self) {
    static Object *toString = NULL;
    if (!toString)
        toString = (Object*) String_fromCharArrayAndSize("toString", 8);

    Object *repr = instance_getattr(self, toString);
    if (repr != LoxNIL) {
        assert(Function_isCallable(repr));
        return repr->type->call(repr, NULL, self, LoxNIL);
    }

    {
        char buffer[64];
        int bytes;
        bytes = snprintf(buffer, sizeof(buffer), "instance@%p", self);
        return (Object*) String_fromCharArrayAndSize(buffer, bytes);
    }
}

static struct object_type InstanceType = (ObjectType) {
    .code = TYPE_OBJECT,
    .name = "object",

    .as_string = instance_asstring,
    .op_eq = IDENTITY,
    
    .getattr = instance_getattr,
    .setattr = instance_setattr,
};


Object*
BoundMethod_create(Object *method, Object *object) {
    assert(Function_isCallable(method));

    BoundMethodObject* O = object_new(sizeof(BoundMethodObject), &BoundMethodType);
    O->method = method;
    O->object = object;
    return (Object*) O;
}

static Object*
boundmethod_invoke(Object* self, VmScope *scope, Object* object, Object* args) {
    assert(self);
    assert(self->type == &BoundMethodType);
    
    BoundMethodObject *this = (BoundMethodObject*) self;
    assert(this->method);
    assert(this->method->type->call);
    return this->method->type->call(this->method, scope, this->object, args);
}

static struct object_type BoundMethodType = (ObjectType) {
    .code = TYPE_BOUND_METHOD,
    .name = "method",

    .op_eq = IDENTITY,

    .call = boundmethod_invoke,
};