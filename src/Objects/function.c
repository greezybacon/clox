#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "boolean.h"
#include "function.h"
#include "string.h"
#include "tuple.h"
#include "Compile/vm.h"
#include "Parse/debug_parse.h"

static struct object_type FunctionType;

bool
Function_isCallable(Object* object) {
    if (!object)
        return false;

    assert(object->type);

    return object->type->call != NULL;
}

bool
Function_isFunction(Object* object) {
    assert(object != NULL);

    return object->type == &FunctionType;
}

Object*
Function_fromAST(ASTFunction* fun) {
    FunctionObject* O = object_new(sizeof(FunctionObject), &FunctionType);
    O->code = fun->block;

    ASTNode *p;
    ASTFuncParam *param;

    int count = 0;
    for (p = fun->arglist; p != NULL; p = p->next)
        count++;

    if (count > 0) {
        LoxString** names = calloc(count, sizeof(LoxString*));
        LoxString** pname = names;
        for (p = fun->arglist; p != NULL; p = p->next) {
            assert(p->type == AST_PARAM);
            param = (ASTFuncParam*) p;
            *pname++ = String_fromCharArrayAndSize(param->name,
                param->name_length);
        }
        O->parameters = (Object**) names;
        O->nparameters = count;
    }

    if (fun->name_length)
        O->name = (Object*) String_fromCharArrayAndSize(fun->name, fun->name_length);

    return (Object*) O;
}

static Object*
function_asstring(Object* self) {
    assert(self->type == &FunctionType);

    char buffer[256];
    int bytes;
    FunctionObject* F = (FunctionObject*) self;
    if (F->name != NULL) {
        assert(String_isString(F->name));
        LoxString* S = (LoxString*) F->name;
        bytes = snprintf(buffer, sizeof(buffer), "function<%.*s>@%p",
            S->length, S->characters, self);
    }
    else
        bytes = snprintf(buffer, sizeof(buffer), "function@%p", self);
    return (Object*) String_fromCharArrayAndSize(buffer, bytes);
}

static struct object_type FunctionType = (ObjectType) {
    .code = TYPE_FUNCTION,
    .name = "function",

    .as_string = function_asstring,
};


static struct object_type NativeFunctionType;

bool
Function_isNativeFunction(Object* object) {
    return object->type == &NativeFunctionType;
}

Object*
NativeFunction_new(NativeFunctionCall callable) {
    LoxNativeFunc* self = object_new(sizeof(LoxNativeFunc), &NativeFunctionType);
    self->callable = callable;
    return (Object*) self;
}

Object*
NativeFunction_bind(Object *self, Object *object) {
    assert(self->type == &NativeFunctionType);

    LoxNativeFunc* this = object_new(sizeof(LoxNativeFunc), &NativeFunctionType);
    this->callable = ((LoxNativeFunc*)self)->callable;
    this->self = object;
    INCREF(object);

    return (Object*) this;
}

static Object*
nfunction_call(Object* self, VmScope *scope, Object* object, Object* args) {
    assert(self->type == &NativeFunctionType);
    return ((LoxNativeFunc*) self)->callable(scope, ((LoxNativeFunc*)self)->self, args);
}

static Object*
nfunction_asstring(Object* self) {
    assert(self->type == &NativeFunctionType);
    return (Object*) String_fromConstant("function() { native code }");
}

static void
nfunction_cleanup(Object* self) {
    assert(self->type == &NativeFunctionType);

    if (((LoxNativeFunc*)self)->self)
        DECREF(((LoxNativeFunc*)self)->self);
}

static struct object_type NativeFunctionType = (ObjectType) {
    .name = "native function",
    .features = FEATURE_STASH,
    .call = nfunction_call,
    .as_string = nfunction_asstring,
    .cleanup = nfunction_cleanup,
};




static struct object_type LoxVmCodeType;

Object*
VmCode_fromContext(ASTFunction *fun, CodeContext *context) {
    LoxVmCode* O = object_new(sizeof(LoxVmCode), &LoxVmCodeType);
    O->context = context;

    if (fun->name_length)
        O->name = (Object*) String_fromCharArrayAndSize(fun->name, fun->name_length);

    return (Object*) O;
}

bool
VmCode_isVmCode(Object *callable) {
    assert(callable);
    assert(callable->type);
    return callable->type == &LoxVmCodeType;
}

Object*
code_asstring(Object* self) {
    assert(self->type == &LoxVmCodeType);

    char buffer[256];
    int bytes;
    LoxVmCode* F = (LoxVmCode*) self;
    if (F->name != NULL) {
        assert(String_isString(F->name));
        LoxString* S = (LoxString*) F->name;
        bytes = snprintf(buffer, sizeof(buffer), "code<%.*s>@%p",
            S->length, S->characters, self);
    }
    else {
        bytes = snprintf(buffer, sizeof(buffer), "code@%p", self);
    }
    return (Object*) String_fromCharArrayAndSize(buffer, bytes);
}

static Object*
codeobject_call(Object* self, VmScope *scope, Object *object, Object *args) {
    assert(self->type == &LoxVmCodeType);
    assert(Tuple_isTuple(args));

    VmEvalContext call_ctx = (VmEvalContext) {
        .code = ((LoxVmCode*) self)->context,
        .scope = scope,
        .this = object,
        .args = (VmCallArgs) {
            .values = ((LoxTuple*) args)->items,
            .count = ((LoxTuple*) args)->count,
        },
    };
    return LoxVM_eval(&call_ctx);
}

static struct object_type LoxVmCodeType = (ObjectType) {
    .name = "code",
    .hash = MYADDRESS,
    .compare = IDENTITY,
    .as_string = code_asstring,
    .call = codeobject_call,
};



static struct object_type VmFunctionObjectType;

static void
vmfun_cleanup(Object* self) {
    assert(self->type == &VmFunctionObjectType);

    LoxVmFunction *this = (LoxVmFunction*) self;
    if (this->scope)
        VmScope_leave(this->scope);
}

static Object*
vmfun_call(Object* self, VmScope *ignored, Object *object, Object *args) {
    assert(self->type == &VmFunctionObjectType);
    assert(Tuple_isTuple(args));

    VmEvalContext call_ctx = (VmEvalContext) {
        .code = ((LoxVmFunction*) self)->code->context,
        .scope = ((LoxVmFunction*) self)->scope,
        .this = object,
        .args = (VmCallArgs) {
            .values = ((LoxTuple*) args)->items,
            .count = ((LoxTuple*) args)->count,
        },
    };
    return LoxVM_eval(&call_ctx);
}

static Object*
vmfun_asstring(Object *self) {
    return (Object*) String_fromConstant("function() {}");
}

LoxVmFunction*
VmCode_makeFunction(Object *code, VmScope *scope) {
    assert(code->type == &LoxVmCodeType);

    LoxVmFunction* O = object_new(sizeof(LoxVmFunction), &VmFunctionObjectType);
    O->code = (LoxVmCode*) code;
    O->scope = scope;

    return O;
}

bool
VmFunction_isVmFunction(Object *callable) {
    assert(callable);
    assert(callable->type);
    return callable->type == &VmFunctionObjectType;
}

static struct object_type VmFunctionObjectType = (ObjectType) {
    .name = "function",
    .hash = MYADDRESS,
    .compare = IDENTITY,
    .call = vmfun_call,
    .cleanup = vmfun_cleanup,
    .as_string = vmfun_asstring,
};