#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "Objects/hash.h"
#include "Objects/object.h"
#include "Parse/parse.h"

#include "scope.h"
#include "stackframe.h"

typedef struct interp_context {
    Parser*     parser;
    Object*     (*eval)(struct interp_context*);

    HashObject  *globals;
    StackFrame  *stack;
} Interpreter;

// From control.c
Object*
eval_if(Interpreter*, ASTIf*);

Object*
eval_while(Interpreter*, ASTWhile*);

Object*
eval_node(Interpreter*, ASTNode*);

// From expression.c

Object*
eval_expression(Interpreter*, ASTExpression*);

Object*
eval_assignment(Interpreter*, ASTAssignment*);

Object*
eval_term(Interpreter*, ASTTerm*);

Object*
eval_lookup(Interpreter*, ASTLookup*);

// From function.c

Object*
eval_invoke(Interpreter*, ASTInvoke*);

Object*
eval_function(Interpreter*, ASTFunction*);

// From stackframe.h

StackFrame* StackFrame_new(void);
void StackFrame_pop(Interpreter*);
StackFrame* StackFrame_push(Interpreter*);

Object*
StackFrame_lookup(StackFrame*, Object*);

void StackFrame_assign_local(StackFrame *self, Object *name, Object *value);
void StackFrame_assign_local(StackFrame *self, Object *name, Object *value);
Scope* StackFrame_createScope(StackFrame*);
StackFrame* StackFrame_create(StackFrame*);

// From scope.h

Scope*
Scope_create(Scope*, HashObject* );

Scope*
Scope_leave(Scope* );

Object*
Scope_lookup(Scope* , Object* );

void
Scope_assign_local(Scope* , Object* , Object* );

void
Scope_assign(Scope* , Object* , Object* );

#endif