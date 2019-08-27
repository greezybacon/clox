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

LoxClass*
Class_build(LoxTable *attributes, LoxClass *parent) {
    LoxClass* O = object_new(sizeof(LoxClass), &ClassType);

    O->attributes = attributes;
    INCREF(attributes);

    O->parent = parent;
    if (parent)
        INCREF((Object*) parent);

    O->name = LoxUndefined;

    // Attach all the callable attributes to this class
    Iterator* it = Hash_getIterator(attributes);
    Object *value, *next;
    while (LoxStopIteration != (next = it->next(it))) {
        assert(Tuple_isTuple(next));
        value = Tuple_GETITEM((LoxTuple*) next, 1);

        if (VmCode_isVmCode(value)) {
            ((LoxVmCode*) value)->context->owner = (Object*) O;
        }
    }
    LoxObject_Cleanup(it);

    return O;
}

bool
Class_isClass(Object* self) {
    assert(self);
    return(self->type == &ClassType);
}

static Object*
class_getattr(Object *self, Object *name, hashval_t hash) {
    assert(self);
    assert(self->type == &ClassType);

    LoxClass *class = (LoxClass*) self;
    Object *method;
    if (class->attributes && (method = Hash_getItemEx(class->attributes, name, hash)))
        return method;

    if (class->parent) {
        return class_getattr((Object*) class->parent, name, hash);
    }

    return LoxUndefined;
}

static void
class_setattr(Object *self, Object *name, Object *value, hashval_t hash) {
    assert(self);
    assert(self->type == &ClassType);

    LoxClass *this = (LoxClass*) self;

    if (!this->attributes)
        this->attributes = Hash_new();

    Hash_setItemEx(this->attributes, name, value, hash);
}

static Object*
class_instanciate(Object* self, VmScope *scope, Object* object, Object* args) {
    assert(self);
    assert(self->type == &ClassType);

    // Create/return a object with (self) as the class
    LoxInstance *O = object_new(sizeof(LoxInstance), &InstanceType);

    O->class = (LoxClass*) self;
    INCREF(self);

    // Call constructor with args
    static Object *init = NULL;
    if (!init)
        init = (Object*) String_fromConstant("init");

    Object *constructor = class_getattr(self, init, HASHVAL(init));
    if (constructor != LoxUndefined) {
        assert(Function_isCallable(constructor));
        // XXX: This is terrible, but INCREF() and DECREF() won't work
        // because the object refcount starts at zero, so it would return
        // to zero and then be cleaned up..
        O->base.protect_delete = true;
        constructor->type->call(constructor, scope, (Object*) O, args);
        O->base.protect_delete = false;
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

static void
class_cleanup(Object *self) {
    assert(self->type == &ClassType);

    LoxClass *this = (LoxClass*) self;
    if (this->parent)
        DECREF((Object*) this->parent);
}

static struct object_type ClassType = (ObjectType) {
    .code = TYPE_CLASS,
    .name = "class",
    .hash = MYADDRESS,
    .cleanup = class_cleanup,

    .as_string = class_asstring,

    .compare = IDENTITY,
    .getattr = class_getattr,
    .setattr = class_setattr,

    .call = class_instanciate,
};

static Object*
instance_getattr(Object *self, Object *name, hashval_t hash) {
    assert(self);
    assert(self->type == &InstanceType);

    LoxInstance *this = (LoxInstance*) self;
    assert(this->class);

    Object *attr;

    if (this->attributes && (attr = Hash_getItemEx(this->attributes, name, hash)))
        return attr;

    if ((attr = class_getattr((Object*) this->class, name, hash)) != LoxUndefined)
        return BoundMethod_create(attr, self);

    // OOPS. Log something!
    return LoxUndefined;
}

static void
instance_setattr(Object *self, Object *name, Object *value, hashval_t hash) {
    assert(self);
    assert(self->type == &InstanceType);

    LoxInstance *this = (LoxInstance*) self;

    if (!this->attributes)
        this->attributes = Hash_new();

    Hash_setItemEx(this->attributes, name, value, hash);
}

static Object*
instance_asstring(Object *self) {
    assert(self->type == &InstanceType);

    static Object *toString = NULL;
    if (!toString)
        toString = (Object*) String_fromConstant("toString");

    Object *repr = instance_getattr(self, toString, HASHVAL(toString));
    if (Function_isCallable(repr)) {
        return repr->type->call(repr, NULL, self, (Object*) LoxEmptyTuple);
    }

    {
        char buffer[64];
        int bytes;
        bytes = snprintf(buffer, sizeof(buffer), "instance@%p", self);
        return (Object*) String_fromCharArrayAndSize(buffer, bytes);
    }
}

static void
instance_cleanup(Object* self) {
    assert(self->type == &InstanceType);

    LoxInstance *this = (LoxInstance*) self;
    DECREF((Object*) this->class);
}

static struct object_type InstanceType = (ObjectType) {
    .code = TYPE_OBJECT,
    .name = "object",
    .cleanup = instance_cleanup,

    .as_string = instance_asstring,
    .compare = IDENTITY,

    .getattr = instance_getattr,
    .setattr = instance_setattr,
};


Object*
BoundMethod_create(Object *method, Object *object) {
    assert(Function_isCallable(method));

    LoxBoundMethod* O = object_new(sizeof(LoxBoundMethod), &BoundMethodType);
    O->method = method;
    O->object = object;
    INCREF(method);
    INCREF(object);

    return (Object*) O;
}

static Object*
boundmethod_invoke(Object* self, VmScope *scope, Object* object, Object* args) {
    assert(self);
    assert(self->type == &BoundMethodType);

    LoxBoundMethod *this = (LoxBoundMethod*) self;
    assert(this->method);
    assert(this->method->type->call);
    return this->method->type->call(this->method, scope, this->object, args);
}

static void
boundmethod_cleanup(Object* self) {
    assert(self->type == &BoundMethodType);

    LoxBoundMethod *this = (LoxBoundMethod*) self;
    DECREF(this->method);
    DECREF(this->object);
}

static struct object_type BoundMethodType = (ObjectType) {
    .code = TYPE_BOUND_METHOD,
    .name = "method",
    .cleanup = boundmethod_cleanup,

    .compare = IDENTITY,

    .call = boundmethod_invoke,
};