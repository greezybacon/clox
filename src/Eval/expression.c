#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "interpreter.h"
#include "Include/Lox.h"
#include "Parse/parse.h"
#include "Parse/debug_parse.h"

// Simple stack for shunting yard algorithm style expression interpretation

typedef struct stack_entry {
    struct stack_entry * next;
    Object* object;
} StackEntry;

typedef struct stack_thingy {
    struct stack_entry * head;

    Object* (*pop)(struct stack_thingy*);
    void (*push)(struct stack_thingy*, Object*);
} Stack;

static Object*
stack_pop(Stack* self) {
    if (self->head == NULL)
        return NULL;

    Object* rv = self->head->object;
    free(self->head);

    self->head = self->head->next;    
    return rv;
}

static void
stack_push(Stack* self, Object* object) {
    StackEntry* new = calloc(1, sizeof(StackEntry));
    if (new == NULL)
        return;

    *new = (StackEntry) {
        .next = self->head,
        .object = object,
    };

    self->head = new;
}

static void
stack_init(Stack* stack) {
    *stack = (Stack) {
        .pop = stack_pop,
        .push = stack_push,
        .head = NULL,
    };
}

// Operator precedence listing (XXX: Maybe place in a header?)

static struct operator_precedence {
    enum token_type operator;
    int             precedence;
} OperatorPrecedence[] = {
    { T_OP_ASSIGN, 10 },
    { T_OP_GT, 20 },
    { T_OP_GTE, 20 },
    { T_OP_LT, 20 },
    { T_OP_LTE, 20 },
    { T_BANG, 30 },
    { T_AND, 30 },
    { T_OR, 30 },
    { T_OP_EQUAL, 40 },
    { T_OP_PLUS, 50 },
    { T_OP_MINUS, 50 },
    { T_OP_STAR, 60 },
    { T_OP_SLASH, 60 },
    { 0, 0 },
};

static inline int
get_precedence(enum token_type op) {
    struct operator_precedence *P = OperatorPrecedence,
        *Pl = NULL;
    for (; P->operator != 0; P++) {
        if (P->operator == op)
            return P->precedence;
    }
    fprintf(stdout, "Huh %d\n", op);
    return 0;
}

static Object*
eval_binary_op(Interpreter* self, Object* arg1, Object* arg2, enum token_type op) {
    Object* rv;

    switch (op) {
    case T_OP_PLUS:
        if (!arg2->type->op_plus)
            eval_error(self, "Object does not support `+`");
        rv = arg2->type->op_plus(arg2, arg1);
        DECREF(arg2);
        DECREF(arg1);
        return rv;
        break;

    case T_OP_MINUS:
        if (!arg2->type->op_minus)
            eval_error(self, "Object does not support `-`");
        return arg2->type->op_minus(arg2, arg1);

    case T_OP_STAR:
        if (!arg2->type->op_star)
            eval_error(self, "Object does not support `*`");
        return arg2->type->op_star(arg2, arg1);

    case T_OP_SLASH:
        if (!arg2->type->op_slash)
            eval_error(self, "Object does not support `/`");
        return arg2->type->op_slash(arg2, arg1);

    case T_OP_LT:
    case T_OP_LTE:
    case T_OP_GT:
    case T_OP_GTE:
    case T_OP_EQUAL:
    case T_OP_ASSIGN:
    case T_BANG:
    case T_AND:
    case T_OR:
        break;
    }
}

Object*
eval_expression(Interpreter* self, ASTExpressionChain* expr) {
    // This uses the shunting yard algorithm here and in parsing to avoid
    // recursion issues.
    ASTExpressionChain *prev = NULL, *current = expr;

    print_node(stderr, (ASTNode*) expr);
    fprintf(stderr, "\n");

    Stack _stack, *stack = &_stack;
    stack_init(stack);

    do {
        stack->push(stack, eval_node(self, current->term));
        if (current->op) {
            current->precedence = get_precedence(current->op);
            while (prev && prev->precedence > current->precedence) {
                // Emit previous operator
                // LHS is PREV
                // RHS is CURRENT
                stack->push(stack, eval_binary_op(self, stack->pop(stack), 
                    stack->pop(stack), prev->op));
                // In the prev chain, bypass this EXPR so that it is not emitted 
                // again below
                prev = prev->prev;
            }
            current->prev = prev;
            prev = current;
        }
        current = (ASTExpressionChain*) ((ASTNode*) current)->next;
    }
    while (current);
    
    while (prev) {
        if (prev->op)
            stack->push(stack, eval_binary_op(self, stack->pop(stack),
                stack->pop(stack), prev->op));
        prev = prev->prev;
    }
    
    Object* rv = stack->pop(stack);
    assert(stack->head == NULL);

    return rv;
}

Object*
eval_term(Interpreter* self, ASTTerm* term) {
    Object* rv;
    switch (term->token_type) {
    case T_NUMBER:
        if (term->isreal) {
            rv = (Object*) Float_fromLongDouble(term->token.real);
        }
        else {
            rv = (Object*) Integer_fromLongLong(term->token.integer);
        }
        break;
    case T_STRING:
        rv = (Object*) String_fromCharArrayAndSize(term->text, term->length);
        break;
    case T_NULL:
    // return LoxNIL;
        fprintf(stdout, "%s", "PUSH (null)\n");
        break;
    case T_TRUE:
    // return LoxTRUE;
        fprintf(stdout, "%s", "PUSH (true)\n");
        break;
    case T_FALSE:
    // return LoxFALSE;
        break;
    case T_WORD:
        return self->lookup(self, term->text, term->length);
    default:
    fprintf(stdout, "%s: %d\n%s", "Say what?", term->token_type,
        (void*) ((ASTNode*) NULL)->next);
    }

    INCREF(rv);
    return rv;
}
