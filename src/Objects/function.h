#ifndef FUNCTION_H
#define FUNCTION_H

#include <stdbool.h>

#include "tuple.h"
#include "Parse/parse.h"
#include "Eval/scope.h"

typedef struct function_object {
    // Inherits from Object
    Object      base;

    ASTNode     *code;
    Object      **parameters;
    size_t      nparameters;
    Scope       *enclosing_scope;
} FunctionObject;

bool Function_isFunction(Object*);
bool Function_isCallable(Object*);

Object*
Function_fromAST(ASTFunction*);

typedef Object* (*NativeFunctionCall)(Interpreter*, Object*, Object*) ;

typedef struct nfunction_object {
    // Inherits from Object
    Object          base;
    NativeFunctionCall callable;
} NFunctionObject;

Object* NativeFunction_new(NativeFunctionCall);

#endif