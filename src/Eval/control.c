#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "interpreter.h"
#include "Include/Lox.h"

Object*
eval_if(Interpreter *self, ASTIf *node) {
    assert(node->node.type == AST_IF);

    Object *condition = eval_node(self, node->condition);
    if (!Bool_isBool(condition)) {
        Object* T = Bool_fromObject(condition);
        DECREF(condition);
        condition = T;
    }

    if (condition == (Object*) LoxTRUE) {
        // Execute the associated block
        return eval_node(self, node->block);
    }
    else if (node->otherwise) {
        return eval_node(self, node->otherwise);
    }

    return LoxNIL;
}

Object*
eval_while(Interpreter *self, ASTWhile* node) {
    assert(node->node.type == AST_WHILE);

    Object *condition;
    Object *rv = LoxNIL;

    for (;;) {
        condition = eval_node(self, node->condition);
        if (!Bool_isBool(condition)) {
            Object* T = Bool_fromObject(condition);
            DECREF(condition);
            condition = T;
        }

        if (condition == (Object*) LoxFALSE)
            break;

        // Execute the associated block
        rv = eval_node(self, node->block);
    }

    return rv;
}