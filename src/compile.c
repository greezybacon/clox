#include <assert.h>

#include "compile.h"
#include "debug_parse.h"
#include "debug_token.h"
#include "parse.h"
#include "vm.h"

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

static void compile_node(ASTNode* node);

static inline int
compare_operators(enum token_type left, enum token_type right) {
    struct operator_precedence *P = OperatorPrecedence,
        *Pl = NULL, *Pr = NULL;
    if (left == right)
        return 0;
    for (; P->operator != 0 && (Pl == NULL || Pr == NULL); P++) {
        if (P->operator == left)
            Pl = P;
        if (P->operator == right)
            Pr = P;
    }
    if (Pl == NULL || Pr == NULL) {
        fprintf(stdout, "Huh\n");
    }
    return Pl->precedence - Pr->precedence;
}

static inline int
get_precedence(enum token_type op) {
    struct operator_precedence *P = OperatorPrecedence,
        *Pl = NULL;
    for (; P->operator != 0; P++) {
        if (P->operator == op)
            return P->precedence;
    }
    fprintf(stdout, "Huh %s\n", get_token_type(op));
    return 0;
}

static void
compile_term(ASTTerm* term) {
    switch (term->token_type) {
    case T_NUMBER:
        if (term->isreal) {
            fprintf(stdout, "PUSH (float) %Lf\n", term->token.real);
        }
        else {
            fprintf(stdout, "PUSH (int) %lld\n", term->token.integer);
        }
        break;
    case T_STRING:
        fprintf(stdout, "PUSH (string) %.*s\n", term->length, term->text);
        break;
    case T_NULL:
        fprintf(stdout, "%s", "PUSH (null)\n");
        break;
    case T_TRUE:
        fprintf(stdout, "%s", "PUSH (true)\n");
        break;
    case T_FALSE:
        fprintf(stdout, "%s", "PUSH (false\n)");
        break;
    default:
    fprintf(stdout, "%s: %s\n%s", "Say what?", get_token_type(term->token_type),
        (void*) ((ASTNode*) NULL)->next);
    }
}

/**
 * The parser will break an expression of "3+5*8" into
 *
 * EXPR --(next)--> EXPR --(next)--> EXPR --(next=NULL)--
 *  \- op => +       \- op => *       \- op => 0
 *  \- term => 3     \- term => 5     \- term => 8
 *
 * A stack based-vm would need to execute something like this
 *
 * PUSH 3
 * PUSH 5
 * PUSH 8
 * BINARY_MUL
 * BINARY_ADD
 */
static void
compile_expression(ASTExpressionChain* expr) {
    // TODO: Consider the shunting yard algorithm here and in parsing to avoid
    //       recursion issues.
    ASTExpressionChain *prev = NULL, *current = expr;
    do {
        compile_node(current->term);
        if (current->op) {
            current->precedence = get_precedence(current->op);
            while (prev && prev->precedence > current->precedence) {
                // Emit previous operator
                fprintf(stdout, "BINOP (%s)\n", get_operator(prev->op));
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
            fprintf(stdout, "BINOP (%s)\n", get_operator(prev->op));
        prev = prev->prev;
    }
    
}

static void
compile_binary_op(ASTBinaryOp* binop) {
    // TODO: Consider the shunting yard algorithm here and in parsing to avoid
    //       recursion issues.
    assert(binop->rhs->type == AST_EXPRESSION);
    ASTExpression* rhs = binop->rhs;
    if (rhs->term->type == AST_BINARY_OP
        && compare_operators(binop->op, ((ASTBinaryOp*) rhs->term)->op) < 0
    ) {
        // This means RHS is lower precedence. Compile it first
        compile_binary_op((ASTBinaryOp*) rhs->term);
        compile_node(binop->lhs);
    // TODO: Consider right associativity like ASSIGN
    }
    else {
        compile_node(binop->lhs);
        compile_node(binop->rhs);
    }
    fprintf(stdout, "BINOP (%s)\n", get_operator(binop->op));
}

static void
compile_node(ASTNode* node) {    
    ASTNode* current = node;
    for (; current; current = current->next) {
        switch (current->type) {
        case AST_EXPRESSION_CHAIN:
            return compile_expression((ASTExpressionChain*) current);
            break;
        case AST_STATEMENT:
        case AST_FUNCTION:
        case AST_WHILE:
        case AST_FOR:
        case AST_IF:
        case AST_VAR:
        case AST_BINARY_OP:
            compile_binary_op((ASTBinaryOp*) current);
            break;
        case AST_TERM:
            compile_term((ASTTerm*) current);
            break;
        case AST_CALL:
            break;
        }
    }
}

static void
compiler_compile(Compiler* compiler, Parser* parser) {
    ASTNode* next;
    while ((next = parser->next(parser)) != NULL) {
        print_node(stderr, next);
        fprintf(stderr, "\n");
        compile_node(next);
    }
}

int
compiler_init(Compiler* compiler) {
    *compiler = (Compiler) {
        .compile = compiler_compile,
    };
    return 0;
}
