#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "object.h"
#include "class.h"
#include "hash.h"
#include "string.h"
#include "function.h"

#include "Vendor/bdwgc/include/gc.h"

static struct object_type ClassType;
static struct object_type InstanceType;
static struct object_type BoundMethodType;

ClassObject*
Class_build(HashObject *attributes) {
    ClassObject* O = object_new(sizeof(ClassObject), &ClassType);
    
    O->attributes = attributes;
    return O;
}

static Object*
class_instanciate(Object* self, VmScope *scope, Object* object, Object* args) {
    assert(self);
    assert(self->type == &ClassType);
    // TODO: Create/return a object with (self) as the class
    InstanceObject *O = object_new(sizeof(InstanceObject), &InstanceType);
    
    O->class = (ClassObject*) self;
    // TODO: Call constructor with args
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

    .call = class_instanciate,
};


static Object*
instance_getattr(Object *self, Object *name) {
    assert(self);
    assert(self->type == &InstanceType);

    InstanceObject *this = (InstanceObject*) self;
    assert(this->class);

    Object *method;

    if ((this->attributes && (method = Hash_getItem(this->attributes, name)) != NULL)
        || (method = Hash_getItem(this->class->attributes, name)) != NULL)
        return BoundMethod_create(method, self);

    // OOPS. Log something!
    return LoxNIL;
}

static struct object_type InstanceType = (ObjectType) {
    .code = TYPE_OBJECT,
    .name = "object",

    .op_eq = IDENTITY,
    
    .getattr = instance_getattr,
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