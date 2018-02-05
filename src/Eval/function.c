#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "interpreter.h"
#include "Include/Lox.h"
#include "scope.h"

Object*
eval_invoke(Interpreter* self, ASTInvoke* invoke) {
    // Fetch the function code to invoke
    assert(invoke->callable);

    // If invoking an expression, then eval it first
    Object* callable = eval_node(self, invoke->callable);

    // XXX: For now this will only work with an actual FunctionObject
    assert(Function_isCallable(callable));
    assert(Function_isFunction(callable));
    
    // Create a new stack frame for the call
    StackFrame *newstack = StackFrame_create(self->stack);

    // Associate closure information, if any
    FunctionObject* F = (FunctionObject*) callable;
    Scope* scope = newstack->scope = F->enclosing_scope;

    // Create local variables from invoke->args
    ASTNode* a = invoke->args;
    Object** param_names = F->parameters;
    Object* T;
    for (; a != NULL; a = a->next, param_names++) {
        T = eval_node(self, a);
        StackFrame_assign_local(newstack, *param_names, T);
        DECREF(T);
    }
    
    // And transition to the new stack for the call
    self->stack = newstack;

    // Eval the function's code (AST)
    Object* result = callable->type->call(callable, (void*) self);
    // XXX: Why is this required?
    INCREF(result);

    StackFrame_pop(self);
    
    return result;
}

Object*
eval_function(Interpreter* self, ASTFunction* callable) {
    // Creates a Function object which can be called with args later
    FunctionObject* F = (FunctionObject*) Function_fromAST(callable);
    Scope* scope = F->enclosing_scope = StackFrame_createScope(self->stack);

    // If the function has a name, attach it to the global? scope
    if (callable->name) {
        Object* key = (Object*) String_fromCharArrayAndSize(callable->name,
            callable->name_length);
        StackFrame_assign_local(self->stack, key, (Object*) F);
        DECREF(key);
    }

    return (Object*) F;
}