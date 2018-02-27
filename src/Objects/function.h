#ifndef FUNCTION_H
#define FUNCTION_H

#include <stdbool.h>

#include "tuple.h"
#include "Parse/parse.h"
#include "Eval/scope.h"
#include "Compile/vm.h"

typedef struct code_context CodeContext;
typedef struct vmeval_scope VmScope;

typedef struct function_object {
    // Inherits from Object
    Object      base;

    Object      *name;
    ASTNode     *code;
    Object      **parameters;
    size_t      nparameters;
    Scope       *enclosing_scope;
} FunctionObject;

bool Function_isFunction(Object*);
bool Function_isCallable(Object*);

Object* Function_fromAST(ASTFunction*);

typedef struct code_object {
    // Inherits from Object
    Object      base;

    Object      *name;
    CodeContext *code;
} CodeObject;

Object* CodeObject_fromContext(ASTFunction*, CodeContext*);

typedef struct vmfunction_object {
    Object      base;
    CodeObject  *code;
    VmScope     *scope;
} VmFunction;

VmFunction* CodeObject_makeFunction(Object*, VmScope*);

typedef Object* (*NativeFunctionCall)(VmScope*, Object*, Object*) ;

typedef struct nfunction_object {
    // Inherits from Object
    Object          base;
    NativeFunctionCall callable;
} NFunctionObject;

Object* NativeFunction_new(NativeFunctionCall);

#endif