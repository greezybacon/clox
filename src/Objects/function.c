#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "function.h"
#include "garbage.h"
#include "string.h"
#include "tuple.h"
#include "Eval/interpreter.h"
#include "Parse/debug_parse.h"

static struct object_type FunctionType;

bool
Function_isCallable(Object* object) {
    if (!object)
        return false;
    
    assert(object->type);
    
    return (object->type->call != NULL);
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
        O->name = String_fromCharArrayAndSize(fun->name, fun->name_length);

    return (Object*) O;
}

static Object*
function_call(Object* self, Interpreter* eval, Object* object, Object* args) {
    assert(self->type == &FunctionType);
    assert(Tuple_isTuple(args));

    // XXX: Assumes params and args have same length. This should handle
    // differing lengths by using the default value or assigning NIL or
    // undefined
    FunctionObject* fun = (FunctionObject*) self;
    Object** param_names = fun->parameters;
    size_t i = 0, count = Tuple_getSize(args);

    for (; i < count; i++, param_names++) {
        StackFrame_assign_local(eval->stack, *param_names,
            Tuple_getItem((TupleObject*) args, i));
    }

    // The idea is that the current stack frame for this invocation has
    // already been setup in the interpreter (before this call)
    return eval_node(eval, ((FunctionObject*) self)->code);
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

    .call = function_call,
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
nfunction_call(Object* self, Interpreter* eval, Object* object, Object* args) {
    assert(self->type == &NativeFunctionType);
    return ((NFunctionObject*) self)->callable(eval, object, args);
}

static Object*
nfunction_asstring(Object* self) {
    assert(self->type == &NativeFunctionType);
    return String_fromCharArrayAndSize("native code {}", 14);
}

static struct object_type NativeFunctionType = (ObjectType) {
    .name = "native function",
    .call = nfunction_call,
    .as_string = nfunction_asstring,
};