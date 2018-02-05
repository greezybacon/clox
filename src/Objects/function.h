#ifndef FUNCTION_H
#define FUNCTION_H

#include <stdbool.h>

#include "Parse/parse.h"
#include "Eval/scope.h"

typedef struct function_object {
    // Inherits from Object
    Object      base;

    ASTNode     *code;
    Object      **parameters;
    Scope       *enclosing_scope;
} FunctionObject;

bool Function_isFunction(Object*);
bool Function_isCallable(Object*);

Object*
Function_fromAST(ASTFunction*);

#endif