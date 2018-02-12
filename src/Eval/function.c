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

    assert(Function_isCallable(callable));
    
    // Create a new stack frame for the call
    StackFrame *newstack = StackFrame_create(self->stack);

    // Associate closure information, if any
    FunctionObject* F = (FunctionObject*) callable;
    Scope* scope = newstack->scope = F->enclosing_scope;

    // Create an args tuple to pass to the function call. A tuple is used
    // rather than setting the stack variables here to support both user-defined
    // functions as well as native functions.
    TupleObject* args = Tuple_new(invoke->nargs);
    ASTNode* a = invoke->args;
    size_t i = 0;
    Object* T;
    for (; a != NULL; a = a->next) {
        T = eval_node(self, a);
        Tuple_setItem(args, i++, T);
        DECREF(T);
    }
    
    // And transition to the new stack for the call
    self->stack = newstack;

    // Eval the function's code (AST)
    Object* result = callable->type->call(callable, (void*) self, NULL, args);
    // XXX: Why is this required?
    INCREF(result);
    DECREF(args);

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