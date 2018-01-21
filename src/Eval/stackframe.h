#ifndef STACKFRAME_H
#define STACKFRAME_H

#include "Objects/hash.h"
#include "Objects/object.h"

typedef struct interp_stack_frame {
    HashObject*                 locals;
    struct interp_stack_frame*  previous;
    // For AST interpretation, this would be both the original file and
    // location of the parsed code as well as the code to be interpreted
    ASTNode*                    ast_node;
} StackFrame;

StackFrame* StackFrame_new(void);

#endif