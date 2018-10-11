#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "interpreter.h"
#include "Include/Lox.h"
#include "Parse/parse.h"
#include "Parse/debug_parse.h"
#include "Objects/boolean.h"

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
eval_binary_op(Interpreter* self, Object* lhs, Object* rhs, enum token_type op) {
    Object* rv;

    switch (op) {
    case T_OP_PLUS:
        if (!lhs->type->op_plus)
            eval_error(self, "Object does not support `+`");
        return lhs->type->op_plus(lhs, rhs);

    case T_OP_MINUS:
        if (!lhs->type->op_minus)
            eval_error(self, "Object does not support `-`");
        return lhs->type->op_minus(lhs, rhs);

    case T_OP_STAR:
        if (!lhs->type->op_star)
            eval_error(self, "Object does not support `*`");
        return lhs->type->op_star(lhs, rhs);

    case T_OP_SLASH:
        if (!lhs->type->op_slash)
            eval_error(self, "Object does not support `/`");
        return lhs->type->op_slash(lhs, rhs);

    case T_OP_LT:
        return lhs->type->op_lt(lhs, rhs);

    case T_OP_LTE:
        return lhs->type->op_lte(lhs, rhs);

    case T_OP_GT:
        return lhs->type->op_gt(lhs, rhs);

    case T_OP_GTE:
        return lhs->type->op_gte(lhs, rhs);

    case T_OP_EQUAL:
        return lhs->type->op_eq(lhs, rhs);

    case T_OP_ASSIGN:
        // For assignment, the rhs expression should be assigned to the local
        // variable represented by lhs
        assert(String_isString(lhs));
        StackFrame_assign(self->stack, lhs, rhs);
        return rhs;

    case T_AND:
    case T_OR:
        break;
    }
}

static inline Object*
eval_unary_op(Interpreter *self, enum token_type unary_op, Object* value) {
    Object *rv;

    switch (unary_op) {
        // Bitwise NOT?
        case T_BANG:
            rv = (Object*) value->type->as_bool(value);
            return (rv == (Object*) LoxFALSE) ? LoxTRUE : LoxFALSE;
        case T_OP_MINUS:
        case T_OP_PLUS:
            // This is really a noop
            break;
    }
}

Object*
eval_expression(Interpreter* self, ASTExpression* expr) {
    Object* lhs = eval_node(self, expr->lhs);

    if (expr->unary_op) {
        lhs = eval_unary_op(self, expr->unary_op, lhs);
    }

    if (expr->binary_op) {
        lhs = eval_binary_op(self, lhs, eval_node(self, expr->rhs),
            expr->binary_op);
    }

    return lhs;
}

Object*
eval_assignment(Interpreter* self, ASTAssignment* assign) {
    assert(String_isString(assign->name));
    Object *result = eval_node(self, assign->expression);
    StackFrame_assign(self->stack, assign->name, result);
    return result;
}

Object*
eval_lookup(Interpreter* self, ASTLookup* lookup) {
    assert(String_isString(lookup->name));
    return StackFrame_lookup(self->stack, lookup->name);
}

Object*
eval_term(Interpreter* self, ASTTerm* term) {
    Object* rv;

    switch (term->token_type) {
    case T_NUMBER:
        if (term->isreal) {
            return (Object*) Float_fromLongDouble(term->token.real);
        }
        return (Object*) Integer_fromLongLong(term->token.integer);
    case T_STRING:
        return (Object*) String_fromLiteral(term->text, term->length);
    case T_NULL:
        return LoxNIL;
    case T_TRUE:
        return (Object*) LoxTRUE;
    case T_FALSE:
        return (Object*) LoxFALSE;

    default:
        fprintf(stdout, "Say what?: %d\n", term->token_type);
    }
    return LoxNIL;
}
