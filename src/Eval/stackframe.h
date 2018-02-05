#ifndef STACKFRAME_H
#define STACKFRAME_H

#include "scope.h"
#include "Objects/hash.h"
#include "Objects/object.h"

typedef struct interp_stack_frame {
    // This is the scope of the interpreter currently
    struct eval_scope           *scope;
    // Variables local to the stack frame. Closure and global variables are
    // accessible via the scope
    HashObject                  *locals;

    // For AST interpretation, this would be both the original file and
    // location of the parsed code as well as the code to be interpreted
    ASTNode*                    ast_node;

    // Linked list
    struct interp_stack_frame   *prev;
} StackFrame;

#endif