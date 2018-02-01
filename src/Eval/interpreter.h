#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "Objects/hash.h"
#include "Objects/object.h"
#include "Parse/parse.h"

#include "stackframe.h"

typedef struct interp_context {
    Parser*     parser;
    Object*     (*eval)(struct interp_context*);
    Object*     (*lookup)(struct interp_context*, char* name, size_t length);
    Object*     (*lookup2)(struct interp_context*, Object* key);
    void        (*assign)(struct interp_context*, char* name, size_t length, Object*);
    void        (*assign2)(struct interp_context*, Object*, Object*);
    
    HashObject* globals;
    StackFrame* stack;
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

// From function.c

Object*
eval_invoke(Interpreter*, ASTInvoke*);

Object*
eval_function(Interpreter*, ASTFunction*);

// From stackframe.h

StackFrame* StackFrame_new(void);
void StackFrame_pop(Interpreter*);
void StackFrame_push(Interpreter*);

Object*
eval_lookup(Interpreter*, char*, size_t);

Object*
eval_lookup2(Interpreter*, Object*);

void
eval_assign(Interpreter*, char*, size_t, Object*);

void
eval_assign2(Interpreter*, Object*, Object*);

#endif