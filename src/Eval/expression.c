#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "interpreter.h"
#include "Include/Lox.h"
#include "Parse/parse.h"
#include "Parse/debug_parse.h"
#include "Objects/boolean.h"

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
    StackEntry* new = malloc(sizeof(StackEntry));
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
eval_binary_op(Interpreter* self, Object* arg2, Object* arg1, enum token_type op) {
    Object* rv;

    switch (op) {
    case T_OP_PLUS:
        if (!arg1->type->op_plus)
            eval_error(self, "Object does not support `+`");
        rv = arg1->type->op_plus(arg1, arg2);
        DECREF(arg2);
        DECREF(arg1);
        return rv;
        break;

    case T_OP_MINUS:
        if (!arg1->type->op_minus)
            eval_error(self, "Object does not support `-`");
        return arg1->type->op_minus(arg1, arg2);

    case T_OP_STAR:
        if (!arg1->type->op_star)
            eval_error(self, "Object does not support `*`");
        return arg1->type->op_star(arg1, arg2);

    case T_OP_SLASH:
        if (!arg1->type->op_slash)
            eval_error(self, "Object does not support `/`");
        return arg1->type->op_slash(arg1, arg2);

    case T_OP_LT:
        return arg1->type->op_lt(arg1, arg2);

    case T_OP_LTE:
        return arg1->type->op_lte(arg1, arg2);

    case T_OP_GT:
        return arg1->type->op_gt(arg1, arg2);

    case T_OP_GTE:
        return arg1->type->op_gte(arg1, arg2);

    case T_OP_EQUAL:
        return arg1->type->op_eq(arg1, arg2);

    case T_OP_ASSIGN:
        // For assignment, the arg2 expression should be assigned to the local
        // variable represented by arg1
        assert(String_isString(arg1));
        self->assign2(self, arg1, arg2);
        return arg2;

    case T_BANG:
    case T_AND:
    case T_OR:
        break;
    }
}

static Object*
eval_word2string(ASTTerm* term) {
    assert(term->token_type == T_WORD);
    return (Object*) String_fromCharArrayAndSize(term->text, term->length);
}

Object*
eval_expression(Interpreter* self, ASTExpressionChain* expr) {
    // This uses the shunting yard algorithm here and in parsing to avoid
    // recursion issues.
    ASTExpressionChain *prev = NULL, *current = expr;

    Stack _stack, *stack = &_stack;
    stack_init(stack);

    do {
        // For assignment, push the WORD token. For everything else, push
        // the result of evaluation
        stack->push(stack, (current->op == T_OP_ASSIGN)
            ? eval_word2string((ASTTerm*) current->lhs) : eval_node(self, current->lhs));
        if (current->op) {
            current->precedence = get_precedence(current->op);
            while (prev && prev->precedence > current->precedence) {
                // Emit previous operator with the top two stack items
                stack->push(stack, eval_binary_op(self, stack->pop(stack), 
                    stack->pop(stack), prev->op));
                // In the prev chain, bypass this EXPR so that it is not emitted 
                // again below
                prev = prev->prev;
            }
            current->prev = prev;
            prev = current;
        }
        current = current->rhs;
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
        return LoxNIL;
    case T_TRUE:
        return (Object*) LoxTRUE;
    case T_FALSE:
        return (Object*) LoxFALSE;
    case T_WORD:
        return self->lookup(self, term->text, term->length);
    default:
        fprintf(stdout, "Say what?: %d\n", term->token_type);
    }

    INCREF(rv);
    return rv;
}
