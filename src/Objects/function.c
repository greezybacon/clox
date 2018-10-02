#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "boolean.h"
#include "function.h"
#include "garbage.h"
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
        StringObject** names = calloc(count, sizeof(StringObject*));
        StringObject** pname = names;
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
        StringObject* S = (StringObject*) F->name;
        bytes = snprintf(buffer, sizeof(buffer), "function<%.*s>@%p",
            S->length, S->characters, self);
    }
    else
        bytes = snprintf(buffer, sizeof(buffer), "function@%p", self);
    return (Object*) String_fromCharArrayAndSize(buffer, bytes);
}

static void
function_cleanup(Object* self) {
    assert(self->type == &FunctionType);
    FunctionObject* F = (FunctionObject*) self;

    if (F->name)
        DECREF(F->name);
}

static struct object_type FunctionType = (ObjectType) {
    .code = TYPE_FUNCTION,
    .name = "function",

    .as_string = function_asstring,

    .cleanup = function_cleanup,
};


static struct object_type NativeFunctionType;

Object*
NativeFunction_new(NativeFunctionCall callable) {
    NFunctionObject* self = object_new(sizeof(NFunctionObject), &NativeFunctionType);
    self->callable = callable;
    return (Object*) self;
}

static Object*
nfunction_call(Object* self, VmScope *scope, Object* object, Object* args) {
    assert(self->type == &NativeFunctionType);
    return ((NFunctionObject*) self)->callable(scope, object, args);
}

static Object*
nfunction_asstring(Object* self) {
    assert(self->type == &NativeFunctionType);
    return (Object*) String_fromCharArrayAndSize("native code {}", 14);
}

static struct object_type NativeFunctionType = (ObjectType) {
    .name = "native function",
    .call = nfunction_call,
    .as_string = nfunction_asstring,
};




static struct object_type CodeObjectType;

Object*
CodeObject_fromContext(ASTFunction *fun, CodeContext *code) {
    CodeObject* O = object_new(sizeof(CodeObject), &CodeObjectType);
    O->code = code;

    if (fun->name_length)
        O->name = (Object*) String_fromCharArrayAndSize(fun->name, fun->name_length);

    return (Object*) O;
}

Object*
code_asstring(Object* self) {
    assert(self->type == &CodeObjectType);

    char buffer[256];
    int bytes;
    CodeObject* F = (CodeObject*) self;
    if (F->name != NULL) {
        assert(String_isString(F->name));
        StringObject* S = (StringObject*) F->name;
        bytes = snprintf(buffer, sizeof(buffer), "code<%.*s>@%p",
            S->length, S->characters, self);
    }
    else {
        bytes = snprintf(buffer, sizeof(buffer), "code@%p", self);
    }
    return (Object*) String_fromCharArrayAndSize(buffer, bytes);
}

static void
code_cleanup(Object* self) {
    assert(self->type == &CodeObjectType);

    if (((CodeObject*) self)->name)
        DECREF(((CodeObject*) self)->name);
}

static struct object_type CodeObjectType = (ObjectType) {
    .name = "code",
    .op_eq = IDENTITY,
    .as_string = code_asstring,
    .cleanup = code_cleanup,
};



static struct object_type VmFunctionObjectType;

static void
vmfun_cleanup(Object* self) {
    assert(self->type == &VmFunctionObjectType);

    free(((VmFunction*) self)->scope);
}

static Object*
vmfun_call(Object* self, VmScope *ignored, Object *object, Object *args) {
    assert(self->type == &VmFunctionObjectType);
    assert(Tuple_isTuple(args));

    VmEvalContext call_ctx = (VmEvalContext) {
        .code = ((VmFunction*) self)->code->code,
        .scope = ((VmFunction*) self)->scope,
        .args = (VmCallArgs) {
            .values = ((TupleObject*) args)->items,
            .count = ((TupleObject*) args)->count,
        },
    };
    return vmeval_eval(&call_ctx);
}

static Object*
vmfun_asstring(Object *self) {
    return (Object*) String_fromCharArrayAndSize("function() {}", 13);
}

VmFunction*
CodeObject_makeFunction(Object *code, VmScope *scope) {
    assert(code->type == &CodeObjectType);

    VmFunction* O = object_new(sizeof(VmFunction), &VmFunctionObjectType);
    O->code = (CodeObject*) code;
    O->scope = scope;

    return O;
}

static struct object_type VmFunctionObjectType = (ObjectType) {
    .name = "function",
    .op_eq = IDENTITY,
    .call = vmfun_call,
    .cleanup = vmfun_cleanup,
    .as_string = vmfun_asstring,
};