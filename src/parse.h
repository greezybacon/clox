#include "token.h"

#ifndef PARSE_H
#define PARSE_H

enum ast_type {
    AST_STATEMENT = 0,
    AST_EXPRESSION,
    AST_FUNCTION,
    AST_WHILE,
    AST_FOR,
    AST_IF,
    AST_VAR,
    AST_BINARY_OP,
    AST_TERM,
    AST_CALL,
    AST_CLASS,
};

typedef struct ast_node {
    enum ast_type   type;
    int             line, offset;
    struct ast_node *next;
} ASTNode;

typedef struct ast_expression {
    ASTNode             node;
    enum token_type     unary_op;
    struct ast_node     *term;
} ASTExpression;

typedef struct ast_term {
    ASTNode             node;
    enum token_type     token_type;
    char                *text;
    int                 length;
} ASTTerm;

typedef struct ast_call {
    ASTNode             node;
    char                *function_name;
    int                 name_length;
    struct ast_node     *args;
} ASTCall;

typedef struct ast_binary_op {
    ASTNode             node;
    struct ast_node     *lhs;
    enum token_type     op;
    struct ast_node     *rhs;
} ASTBinaryOp;

typedef struct ast_class {
    ASTNode             node;
    char *              name;
    char *              extends;
    struct ast_node*    body;
} ASTClass;

typedef struct ast_fun {
    ASTNode             node;
    char *              name;
    struct ast_node     *arglist;
    struct ast_node     *block;
} ASTFunction;

typedef struct ast_if {
    ASTNode             node;
    struct ast_node*    condition;
    struct ast_node*    block;
} ASTIf;

typedef struct ast_for {
    ASTNode             node;
    struct ast_node*    initializer;
    struct ast_node*    condition;
    struct ast_node*    post_loop;
    struct ast_node*    block;
} ASTFor;

typedef struct ast_var {
    ASTNode             node;
    char *              name;
    struct ast_node*    expression;
} ASTVar;

typedef struct parser_context {
    Tokenizer *     tokens;
    ASTNode*        (*next)(struct parser_context*);
} Parser;


int parser_init(Parser*, Stream*);

#endif
