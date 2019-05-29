#include <stdbool.h>

#include "token.h"
#include "Objects/object.h"

#ifndef PARSE_H
#define PARSE_H

enum ast_type {
    AST_STATEMENT = 0,
    AST_EXPRESSION,
    AST_ASSIGNMENT,
    AST_FUNCTION,
    AST_RETURN,
    AST_PARAM,
    AST_WHILE,
    AST_FOR,
    AST_IF,
    AST_VAR,
    AST_TERM,
    AST_LITERAL,
    AST_LOOKUP,
    AST_INVOKE,
    AST_CLASS,
    AST_ATTRIBUTE,
    AST_MAGIC,
    AST_SLICE,
    AST_TUPLE_LITERAL,
    AST_INTERPOL_STRING,
    AST_INTERPOLATED,
    AST_TABLE_LITERAL,
};

enum associativity {
    ASSOC_LEFT,
    ASSOC_RIGHT,
};

typedef struct ast_node {
    enum ast_type   type;
    unsigned        line, offset;
    struct ast_node *next;
} ASTNode;

typedef struct ast_expression {
    ASTNode             node;
    enum token_type     unary_op;
    enum token_type     binary_op;
    struct ast_node     *lhs;
    struct ast_node     *rhs;
    bool                isreturn;
} ASTExpression;

typedef struct ast_return {
    ASTNode             node;
    struct ast_node     *expression;
} ASTReturn;

typedef struct ast_assignment {
    ASTNode             node;
    Object              *name;
    ASTNode             *expression;
} ASTAssignment;

typedef struct ast_lookup {
    ASTNode             node;
    Object              *name;
    hashval_t           hash;
} ASTLookup;

typedef struct ast_magic {
    ASTNode             node;
    struct {
        unsigned        this:1;
        unsigned        super:1;
    };
} ASTMagic;

typedef struct ast_term {
    ASTNode             node;
    enum token_type     token_type;
    union {
        long long       integer;
        long double     real;
    } token;
    const char          *text;  // Original token text (could be free()d for int/real)
    unsigned            length; // Length of text
    bool                isreal; // T_NUMBER could be float or int
} ASTTerm;

typedef struct ast_literal {
    ASTNode             node;
    Object*             literal;
} ASTLiteral;

typedef struct ast_interpolated_string {
    ASTNode             node;
    ASTNode             *items;
} ASTInterpolatedString;

typedef struct ast_interpolated_expr {
    ASTNode             node;
    ASTNode             *expr;
    const char          *flags;
    const char          *format;
} ASTInterpolatedExpr;

typedef struct ast_invoke {
    ASTNode             node;
    struct ast_node     *callable;
    struct ast_node     *args;
    size_t              nargs;
} ASTInvoke;

typedef struct ast_parameter {
    ASTNode             node;
    const char          *name;
    unsigned            name_length;
    struct ast_node     *default_value;
} ASTFuncParam;

typedef struct ast_slice {
    ASTNode             node;
    struct ast_node     *object;
    struct ast_node     *start;
    struct ast_node     *end;
    struct ast_node     *step;
    struct ast_node     *value;         // Used for item assignment
} ASTSlice;

typedef struct ast_class {
    ASTNode             node;
    Object              *name;
    struct ast_node     *extends;
    struct ast_node     *body;
} ASTClass;

typedef struct ast_fun {
    ASTNode             node;
    const char *        name;
    unsigned            name_length;
    struct ast_node     *arglist;
    struct ast_node     *block;
} ASTFunction;

typedef struct ast_if {
    ASTNode             node;
    struct ast_node*    condition;  // Expression
    struct ast_node*    block;      // Block if condition is true
    struct ast_node*    otherwise;  // Might be IF, might be block
} ASTIf;

typedef struct ast_while {
    ASTNode             node;
    struct ast_node*    condition;
    struct ast_node*    block;
} ASTWhile;

typedef struct ast_for {
    ASTNode             node;
    struct ast_node*    initializer;
    struct ast_node*    condition;
    struct ast_node*    post_loop;
    struct ast_node*    block;
} ASTFor;

typedef struct ast_var {
    ASTNode             node;
    const char *        name;
    size_t              name_length;
    struct ast_node*    expression;
} ASTVar;

typedef struct ast_attribute {
    ASTNode             node;
    ASTNode             *object;
    Object              *attribute;
    ASTNode             *value;
} ASTAttribute;

typedef struct ast_tuple_literal {
    ASTNode             node;
    struct ast_node     *items;
} ASTTupleLiteral;

typedef struct ast_table_literal {
    ASTNode             node;
    ASTNode             *keys;
    ASTNode             *values;
} ASTTableLiteral;

typedef enum {
    LOX_PARSE_NO_AUTO_TUPLE     = 1<<0,
} LoxParserFlag;

typedef struct parser_context {
    Tokenizer *     tokens;
    ASTNode*        previous;
    LoxParserFlag   flags;

    ASTNode*        (*next)(struct parser_context*);
} Parser;


int parser_init(Parser*, Stream*);

#endif
