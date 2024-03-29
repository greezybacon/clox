#ifndef FUNCTION_H
#define FUNCTION_H

#include <stdbool.h>

#include "object.h"
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
bool VmCode_isVmCode(Object *);
bool VmFunction_isVmFunction(Object *);

Object* Function_fromAST(ASTFunction*);

typedef struct code_object {
    // Inherits from Object
    Object      base;

    Object      *name;
    CodeContext *context;
} LoxVmCode;

Object* VmCode_fromContext(ASTFunction*, CodeContext*);

typedef struct vmfunction_object {
    Object      base;
    LoxVmCode  *code;
    VmScope     *scope;
} LoxVmFunction;

LoxVmFunction* VmCode_makeFunction(Object*, VmScope*);

typedef struct nfunction_object {
    // Inherits from Object
    Object          base;
    NativeFunctionCall callable;
    Object          *self;
} LoxNativeFunc;

Object* NativeFunction_new(NativeFunctionCall);
bool Function_isNativeFunction(Object*);
Object* NativeFunction_bind(Object*, Object*);

typedef struct native_property_object {
    // Inherits from Object
    Object          base;
    Object          *getter;
    Object          *setter;
    Object          *delete;
} LoxNativeProperty;

Object* LoxNativeProperty_create(NativeFunctionCall, NativeFunctionCall, NativeFunctionCall);
bool LoxNativeProperty_isProperty(Object*);
Object* LoxNativeProperty_callGetter(Object*, VmScope*, Object*);

#endif