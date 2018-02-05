#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "function.h"
#include "string.h"
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
Function_fromAST(ASTFunction* function) {
    FunctionObject* O = object_new(sizeof(FunctionObject), &FunctionType);
    O->code = function->block;
        
    ASTNode *p;
    ASTFuncParam *param;
    
    int count = 0;
    for (p = function->arglist; p != NULL; p = p->next)
        count++;

    if (count > 0) {
        StringObject** names = calloc(count, sizeof(StringObject*));
        StringObject** pname = names;
        for (p = function->arglist; p != NULL; p = p->next) {
            assert(p->type == AST_PARAM);
            param = (ASTFuncParam*) p;
            *pname++ = String_fromCharArrayAndSize(param->name,
                param->name_length);
        }
        O->parameters = (Object**) names;
    }

    return (Object*) O;
}

static Object*
function_call(Object* self, Interpreter* eval) {
    assert(self->type == &FunctionType);
    
    // The idea is that the current stack frame for this invocation has
    // already been setup in the interpreter (before this call)
    return eval_node(eval, ((FunctionObject*) self)->code);
}

static Object*
function_asstring(Object* self) {
    char buffer[256];
    int bytes;
    bytes = snprintf(buffer, sizeof(buffer), "function@%p", self);
    return (Object*) String_fromCharArrayAndSize(buffer, bytes);
}

static struct object_type FunctionType = (ObjectType) {
    .code = TYPE_FUNCTION,
    .name = "function",

    .call = function_call,
    .as_string = function_asstring,
};