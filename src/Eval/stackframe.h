#ifndef STACKFRAME_H
#define STACKFRAME_H

#include "Objects/hash.h"
#include "Objects/object.h"
#include "scope.h"

typedef struct interp_stack_frame {
    struct eval_scope           *scope;
    // For AST interpretation, this would be both the original file and
    // location of the parsed code as well as the code to be interpreted
    ASTNode*                    ast_node;

    // Linked list
    struct interp_stack_frame   *prev;
} StackFrame;

#endif