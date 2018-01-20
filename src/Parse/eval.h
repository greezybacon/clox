#ifndef EVAL_H
#define EVAL_H

#include "parse.h"
#include "Lib/hash.h"

typedef struct eval_stack_frame {
    HashObject*             locals;
    struct eval_stack_frame* previous;
    ASTNode*                ast_node;
} StackFrame;

typedef struct eval_context {
    Parser *    parser;
    Object*     (*eval)(struct eval_context*);
    Object*     (*lookup)(struct eval_context*, char* name, size_t length);
    
    HashObject* globals;
    StackFrame* stack;
} EvalContext;

#endif