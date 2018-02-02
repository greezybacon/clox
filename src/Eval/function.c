#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "interpreter.h"
#include "Include/Lox.h"
// #include "Parse/parse.h"
// #include "Parse/debug_parse.h"

Object*
eval_invoke(Interpreter* self, ASTInvoke* invoke) {
    // Fetch the function code to invoke
    assert(invoke->callable);

    // If invoking an expression, then eval it first
    Object* callable = eval_node(self, invoke->callable);

    // XXX: For now this will only work with an actual FunctionObject
    assert(Function_isCallable(callable));
    assert(Function_isFunction(callable));

    StackFrame_push(self);

    // TODO: Associate closure information, if any

    // Create local variables from invoke->args
    ASTNode* a = invoke->args;
    Object* T;
    Object** param_names = ((FunctionObject*) callable)->parameters;
    for (; a != NULL; a = a->next, param_names++) {
        // TODO: Stash result of literals (without eval())
        self->assign2(self, *param_names, eval_node(self, a));
    }

    // Eval the function's code (AST)
    Object* result = callable->type->call(callable, (void*) self);
    
    StackFrame_pop(self);
    
    return result;
}

Object*
eval_function(Interpreter* self, ASTFunction* callable) {
    // Creates a Function object which can be called with
    // args later
    Object* F = Function_fromAST(callable);

    // TODO: If the function has a name, attach it to the global? scope
    if (callable->name_length)
        self->assign(callable->name, callable->name_length, F);

    return F;
}