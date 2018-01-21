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
    
    HashObject* globals;
    StackFrame* stack;
} Interpreter;

Object*
eval_node(Interpreter*, ASTNode*);

#endif