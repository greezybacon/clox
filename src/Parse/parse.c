#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_token.h"
#include "debug_parse.h"
#include "Eval/interpreter.h"
#include "parse.h"
#include "Objects/string.h"
#include "Compile/vm.h"

#include "Vendor/bdwgc/include/gc.h"

static ASTNode* parse_expression(Parser*);
static ASTNode* parse_statement(Parser*);
static ASTNode* parse_statement_or_block(Parser*);
static ASTNode* parse_next(Parser*);

static void
parser_node_init(ASTNode* node, int type, Token* token) {
    *node = (ASTNode) {
        .type = type,
        .line = token->line,
        .offset = token->pos,
    };
}

static void
parser_with_flags(Parser* current, Parser *new, LoxParserFlag flags) {
    *new = *current;
    new->flags = flags;
}

static void
parse_syntax_error(Parser* self, char* message) {
    Tokenizer* tokens = self->tokens;
    Token* next = tokens->current;

    fprintf(stderr, "Syntax Error: line %d, at %d: %s\n",
        tokens->stream->line, tokens->stream->offset, message);
    exit(-1);
}

static Token*
parse_expect(Parser* self, enum token_type type) {
    Tokenizer* T = self->tokens;
    Token* next = T->next(T);

    if (next->type != type) {
        char buffer[255];
        snprintf(buffer, sizeof(buffer), "Expected %s(%d), got %s(%d) >%.*s<",
            get_token_type(type), type, get_token_type(next->type), next->type,
            next->length, next->text);
        parse_syntax_error(self, buffer);
    }

    return next;
}

static ASTNode*
parse_block(Parser* self) {
    Tokenizer* T = self->tokens;
    Token* peek;
    ASTNode *result=NULL, *list, *next;

    parse_expect(self, T_OPEN_BRACE);
    do {
        next = parse_next(self);
        if (result == NULL) {
            result = list = next;
        }
        else {
            list = list->next = next;
        }
        peek = T->peek(T);
    }
    while (peek->type != T_EOF && peek->type != T_CLOSE_BRACE);

    parse_expect(self, T_CLOSE_BRACE);

    return result;
}

static ASTNode*
parse_slice(Parser* self, ASTNode* object) {
    Tokenizer* T = self->tokens;
    Token* peek;

    ASTSlice *result = GC_MALLOC(sizeof(ASTSlice));
    parser_node_init((ASTNode*) result, AST_SLICE, T->peek(T));
    result->object = object;

    do {
        // Comsume the open bracket or the colon
        T->next(T);

        if (!result->start)
            result->start = parse_expression(self);
        else if (!result->end)
            result->end = parse_expression(self);
        else if (!result->step)
            result->step = parse_expression(self);
        else
            parse_syntax_error(self, "Too many colons in slice");

        peek = T->peek(T);
    }
    while (peek->type == T_COLON);
    parse_expect(self, T_CLOSE_BRACKET);

    return (ASTNode*) result;
}

static ASTNode*
parse_invoke(Parser* self, ASTNode* callable) {
    Tokenizer* T = self->tokens;
    Token* func = T->current;
    Token* peek = T->peek(T);
    ASTNode* args = NULL;
    ASTNode* narg;
    size_t count = 0;

    ASTInvoke* call = GC_MALLOC(sizeof(ASTInvoke));
    parser_node_init((ASTNode*) call, AST_INVOKE, func);
    call->callable = callable;

    // Don't parse tuples unless enclosed in parentheses
    Parser nested;
    parser_with_flags(self, &nested, self->flags | LOX_PARSE_NO_AUTO_TUPLE);

    do {
        // Comsume the open paren or the comma
        T->next(T);

        // Arguments are optional ...
        if (T->peek(T)->type == T_CLOSE_PAREN)
            break;

        narg = parse_expression(&nested);
        count += 1;
        if (args == NULL) {
            call->args = args = narg;
        }
        else {
            args = args->next = narg;
        }
        peek = T->peek(T);
    }
    while (peek->type == T_COMMA);
    parse_expect(self, T_CLOSE_PAREN);

    call->nargs = count;
    return (ASTNode*) call;
}

/**
 * Collects the names of the arguments defined for a function
 */
static ASTNode*
parse_arg_list(Parser* self) {
    Tokenizer* T = self->tokens;
    Token* next;
    ASTNode *result = NULL, *args = NULL;
    ASTFuncParam *param;

    parse_expect(self, T_OPEN_PAREN);

    for (;;) {
        next = T->peek(T);
        if (next->type == T_CLOSE_PAREN) {
            break;
        }
        else if (next->type == T_COMMA) {
            next = T->next(T);
        }
        next = parse_expect(self, T_WORD);

        param = GC_MALLOC(sizeof(ASTFuncParam));
        parser_node_init((ASTNode*) param, AST_PARAM, next);

        param->name = next->text;
        param->name_length = next->length;

        // Chain the results together
        if (args == NULL) {
            result = args = (ASTNode*) param;
        }
        else {
            args = args->next = (ASTNode*) param;
        }
    }
    return result;
}

static Object*
parse_word2string(ASTNode* node) {
    assert(node->type == AST_LOOKUP);
    Object* rv = ((ASTLookup*)node)->name;
    return rv;
}

static ASTNode* parse_TERM(Parser*);

static inline ASTNode*
parse_expression_attr(Parser *self, ASTNode *lhs) {
    Tokenizer* T = self->tokens;
    Token* next = T->next(T);

    ASTAttribute* attr = GC_MALLOC(sizeof(ASTAttribute));
    parser_node_init((ASTNode*) attr, AST_ATTRIBUTE, next);
    attr->attribute = parse_word2string(parse_TERM(self));
    attr->object = lhs;
    return (ASTNode*) attr;
}

#define IFNULL3(a,b,c) ((a) == NULL ? ((b) == NULL ? (c) : (b)) : (a))
#define IFNULL2(a,b) ((a) == NULL ? (b) : (a))

static inline ASTNode*
parse_interpolated(Parser *self, const char *text, int length, const char **end) {
    const char *start, *flag_start = 0, *format_start = 0;

    start = text;
    while (length--) {
        if (*text == '}')
            break;
        if (*text == '!')
            flag_start = text + 1;
        else if (*text == ':')
            format_start = text + 1;
        text++;
    }
    // Skip closing brace char
    text++;

    Parser eval;
    Stream stream;
    stream_init_buffer(&stream, start, IFNULL3(flag_start, format_start, text) - start);

    // Preseve current location for better error output
    stream.line = self->tokens->stream->line;
    stream.offset = self->tokens->stream->offset;
    parser_init(&eval, &stream);

    ASTNode* expr = parse_expression(&eval);

    ASTInterpolatedExpr *interpol = GC_NEW(ASTInterpolatedExpr);
    parser_node_init((ASTNode*) interpol, AST_INTERPOLATED, self->tokens->current);

    interpol->expr = expr;

    if (format_start) {
        interpol->format = GC_STRNDUP(format_start, text - format_start - 1);
    }

    *end = text;
    return (ASTNode*) interpol;
}

static ASTNode*
parse_string_literal(Parser *self, const char *text, int length) {
    ASTNode *result = NULL, *interpol, *items = NULL;
    ASTLiteral *literal;
    Object *string;
    const char *start = text, *end;
    Token *current = self->tokens->current;

    while (length--) {
        if (*text == '#' && *(text + 1) == '{') {
            if (!result) {
                result = GC_MALLOC(sizeof(ASTInterpolatedString));
                parser_node_init((ASTNode*) result, AST_INTERPOL_STRING, current);
            }

            // XXX: This assumes we will always start with non-zero-width string
            string = (Object*) String_fromLiteral(start, text - start);
            literal = GC_MALLOC(sizeof(ASTLiteral));
            parser_node_init((ASTNode*) literal, AST_LITERAL, current);
            literal->literal = string;

            if (!items) {
                assert(result->type == AST_INTERPOL_STRING);
                ((ASTInterpolatedString*) result)->items = items = (ASTNode*) literal;
            }
            else {
                items = items->next = (ASTNode*) literal;
            }

            // Parse the interpolated expression
            interpol = parse_interpolated(self, text + 2, length, &end);
            items = items->next = interpol;

            length -= end - text - 1;
            start = text = end;
        }
        else
            text++;
    }

    if (text - start) {
        string = (Object*) String_fromLiteral(start, text - start);
        literal = GC_MALLOC(sizeof(ASTLiteral));
        parser_node_init((ASTNode*) literal, AST_LITERAL, current);
        literal->literal = string;

        if (result != NULL && items != NULL) {
            items->next = (ASTNode*) literal;
        }
        else {
            // Plain old string
            result = (ASTNode*) literal;
        }
    }

    return result;
}

static ASTNode*
parse_table_literal(Parser* self) {
    Tokenizer *T = self->tokens;
    ASTNode *key, *value, *keys, *values;

    ASTTableLiteral *table = GC_MALLOC(sizeof(ASTTableLiteral));
    parser_node_init((ASTNode*) table, AST_TABLE_LITERAL, T->current);

    // Commas in this list should not promote this expression to a tuple
    Parser nested;
    parser_with_flags(self, &nested, self->flags | LOX_PARSE_NO_AUTO_TUPLE);

    while (T->peek(T)->type != T_CLOSE_BRACE) {
        key = parse_expression(&nested);
        parse_expect(self, T_COLON);
        value = parse_expression(&nested);

        if (!table->keys)
            keys = table->keys = key;
        else
            keys->next = key;

        if (!table->values)
            values = table->values = value;
        else
            values->next = value;

        if (T->peek(T)->type == T_COMMA)
            T->next(T);
    }
    parse_expect(self, T_CLOSE_BRACE);

    return (ASTNode*) table;
}

static ASTNode*
parse_TERM(Parser* self) {
    Tokenizer* T = self->tokens;
    Token* next = self->tokens->current;
    ASTNode* result = NULL;

    switch (next->type) {
    case T_OPEN_PAREN: {
        // This would be a nested expression. Tuples are also allowed
        Parser nested;
        parser_with_flags(self, &nested, self->flags & ~LOX_PARSE_NO_AUTO_TUPLE);
        if (T->peek(T)->type != T_CLOSE_PAREN) {
            result = (ASTNode*) parse_expression(&nested);
        }
        parse_expect(self, T_CLOSE_PAREN);
        break;
    }
    case T_OPEN_BRACE: {
        // This is for a table literal (keys as colons)
        result = parse_table_literal(self);
        break;
    }
    case T_WORD: {
        ASTLookup *lookup = GC_MALLOC(sizeof(ASTLookup));
        parser_node_init((ASTNode*) lookup, AST_LOOKUP, next);
        lookup->name = (Object*) String_fromCharArrayAndSize(
            self->tokens->fetch_text(self->tokens, next),
            next->length
        );
        lookup->hash = lookup->name->type->hash(lookup->name);
        result = (ASTNode*) lookup;
        break;
    }
    case T_STRING: {
        const char *text = self->tokens->fetch_text(self->tokens, next);
        int length = next->length;
        result = (ASTNode*) parse_string_literal(self, text, length);
        break;
    }
    case T_NUMBER:
    case T_TRUE:
    case T_FALSE:
    case T_NULL: {
        // Do something with the token
        ASTTerm _term, *term = &_term;
        parser_node_init((ASTNode*) term, AST_TERM, next);
        term->token_type = next->type;
        term->text = self->tokens->fetch_text(self->tokens, next);
        term->length = next->length;

        if (next->type == T_NUMBER) {
            char* endpos;
            term->isreal = memchr(term->text, '.', next->length) != NULL;
            if (term->isreal) {
                term->token.real = strtold(term->text, &endpos);
                if (term->token.real == 0.0 && endpos == term->text) {
                    parse_syntax_error(self, "Invalid floating point number");
                }
            }
            else {
                term->token.integer = strtoll(term->text, &endpos, 10);
                if (term->token.integer == 0 && endpos == term->text) {
                    parse_syntax_error(self, "Invalid integer number");
                }
            }
        }

        Object *value = eval_term(NULL, term);
        ASTLiteral* literal = GC_MALLOC(sizeof(ASTLiteral));
        parser_node_init((ASTNode*) literal, AST_LITERAL, next);
        literal->literal = value;
        result = (ASTNode*) literal;
        break;
    }
    case T_THIS:
    case T_SUPER: {
        ASTMagic *lookup = GC_NEW(ASTMagic);
        parser_node_init((ASTNode*) lookup, AST_MAGIC, next);
        lookup->this = next->type == T_THIS ? 1 : 0;
        lookup->super = next->type == T_SUPER ? 1 : 0;
        result = (ASTNode*) lookup;
        break;
    }
    case T_FUNCTION: {
        ASTFunction* astfun = GC_MALLOC(sizeof(ASTFunction));
        parser_node_init((ASTNode*) astfun, AST_FUNCTION, next);

        if (T->peek(T)->type == T_WORD) {
            next = T->next(T);
            // XXX: Use a LoxString for better memory management
            astfun->name = strndup(
                self->tokens->fetch_text(self->tokens, next),
                next->length
            );
            astfun->name_length = next->length;
        }

        astfun->arglist = parse_arg_list(self);
        parse_expect(self, T_CLOSE_PAREN);
        astfun->block = parse_block(self);

        result = (ASTNode*) astfun;
        break;
    }

    default:
        fprintf(stderr, "Parse error: Unexpected TERM token: %d\n", next->type);
    }

    return (ASTNode*) result;
}

// Operator precedence listing (XXX: Maybe place in a header?)
typedef struct operator_info OperatorInfo;
static struct operator_info {
    enum token_type     operator;
    enum associativity  assoc;
    int                 precedence;
    const char          *symbol;
} OperatorPrecedence[] = {
    { T_OP_ASSIGN,  ASSOC_RIGHT,    10, ":=" },
    { T_AND,        ASSOC_LEFT,     20, "and" },
    { T_OR,         ASSOC_LEFT,     20, "or" },
    { T_OP_AMPERSAND, ASSOC_LEFT,   25, "&" },
    { T_OP_PIPE,    ASSOC_LEFT,     25, "|" },
    { T_OP_CARET,   ASSOC_LEFT,     25, "^" },
    { T_OP_EQUAL,   ASSOC_LEFT,     30, "==" },
    { T_OP_GT,      ASSOC_LEFT,     40, ">" },
    { T_OP_GTE,     ASSOC_LEFT,     40, ">=" },
    { T_OP_LT,      ASSOC_LEFT,     40, "<" },
    { T_OP_LTE,     ASSOC_LEFT,     40, "<=" },
    { T_BANG,       ASSOC_LEFT,     45, "!" },
    { T_OP_IN,      ASSOC_LEFT,     45, "in" },
    { T_OP_LSHIFT,  ASSOC_LEFT,     47, "<<" },
    { T_OP_RSHIFT,  ASSOC_LEFT,     47, ">>" },
    { T_OP_PLUS,    ASSOC_LEFT,     50, "+" },
    { T_OP_MINUS,   ASSOC_LEFT,     50, "-" },
    { T_OP_STAR,    ASSOC_LEFT,     60, "*" },
    { T_OP_SLASH,   ASSOC_LEFT,     60, "/" },
    { T_OP_PERCENT, ASSOC_LEFT,     60, "%" },
    { T_OPEN_PAREN, ASSOC_RIGHT,    70, "(" },
    { T_OPEN_BRACKET, ASSOC_RIGHT,  70, "[" },
    { T_DOT,        ASSOC_LEFT,     80, "." },
    { 0, 0, 0 },
};

static inline const OperatorInfo*
get_operator_info(enum token_type op) {
    struct operator_info *P = OperatorPrecedence;
    for (; P->operator != 0; P++) {
        if (P->operator == op)
            return P;
    }
    fprintf(stdout, ">Huh %d\n", op);
    return 0;
}

static inline ASTNode*
parse_expression_assign(Token *token, ASTNode *lhs, ASTNode *rhs) {
    ASTNode *rv;
    if (lhs->type == AST_LOOKUP) {
        ASTAssignment* assign = GC_MALLOC(sizeof(ASTAssignment));
        parser_node_init((ASTNode*) assign, AST_ASSIGNMENT, token);
        assign->expression = rhs;
        assign->name = parse_word2string(lhs);
        rv = (ASTNode*) assign;
    }
    else if (lhs->type == AST_SLICE) {
        ((ASTSlice*) lhs)->value = rhs;
        rv = lhs;
    }
    else if (lhs->type == AST_ATTRIBUTE) {
        ((ASTAttribute*) lhs)->value = rhs;
        rv = lhs;
    }
    else {
        assert(!"Unexpected assignment LHS");
    }

    return rv;
}

static ASTNode*
parse_tuple_items(Parser* self, ASTNode *first) {
    Token *next;
    Tokenizer* T = self->tokens;
    ASTNode *item;

    ASTTupleLiteral *result = GC_MALLOC(sizeof(ASTTupleLiteral));
    parser_node_init((ASTNode*) result, AST_TUPLE_LITERAL, T->peek(T));

    result->items = first;

    for (;;) {
        next = T->peek(T);
        if (next->type == T_CLOSE_PAREN) {
            break;
        }
        else if (next->type == T_COMMA) {
            next = T->next(T);
        }

        item = parse_expression(self);

        // Chain the results together
        first->next = item;
        first = item;
    }
    return (ASTNode*) result;
}

static ASTNode*
parse_expression_r(Parser* self, const OperatorInfo *previous) {
    /* General expression grammar
     * EXPR = UNARY? TERM [ CALL ]* [ OP EXPR ]*
     * TERM = PAREN | FUNCTION | WORD | NUMBER | STRING | TRUE | FALSE | NULL
     * CALL = INVOKE | SLICE
     * PAREN = "(" EXPR ")"
     * OP = T_EQUAL | T_GT | T_GTE | T_LT | T_LTE | T_AND | T_OR | T_ASSIGN | T_DOT
     * UNARY = T_BANG | T_PLUS | T_MINUS
     * FUNCTION = "fun" [ WORD ] "(" [ EXPR ( "," EXPR )* ] ")" BLOCK
     * INVOKE = T_OPEN_PAREN [ EXPR ( ',' EXPR )* ] T_CLOSE_PAREN
     * SLICE = T_OPEN_BRACKET [ EXPR [ ':' EXPR [ ':' EXPR ] ] ] T_CLOSE_BRACKET
     */
    Tokenizer* T = self->tokens;
    Token* next;
    const OperatorInfo *operator;
    ASTNode *lhs = NULL, *rhs;
    ASTExpression *expr;
    enum token_type unary_op = 0;

    // Read next token and handle unary operations
    next = T->next(T);
    if (next->type == T_BANG || next->type == T_OP_PLUS || next->type == T_OP_MINUS) {
        unary_op = next->type;
        lhs = parse_expression_r(self, 0);
        if (lhs->type != AST_EXPRESSION || ((ASTExpression*) lhs)->unary_op) {
            expr = GC_NEW(ASTExpression);
            parser_node_init((ASTNode*) expr, AST_EXPRESSION, next);
            expr->lhs = lhs;
            lhs = (ASTNode*) expr;
        }

        ((ASTExpression*) lhs)->unary_op = unary_op;
        return lhs;
    }

    lhs = parse_TERM(self);
    if (lhs == NULL)
        return NULL;

    // If there is no current expression, then this becomes the LHS
    next = T->peek(T);

    // Handle CALL and SLICE
    for (;;) {
        if (next->type == T_OPEN_PAREN) {
            lhs = parse_invoke(self, lhs);
        }
        else if (next->type == T_OPEN_BRACKET) {
            lhs = parse_slice(self, lhs);
        }
        else if (next->type == T_DOT) {
            // Consume the T_DOT
            T->next(T);
            lhs = parse_expression_attr(self, lhs);
        }
        else if (next->type == T_COMMA
            && !(self->flags & LOX_PARSE_NO_AUTO_TUPLE)
         ) {
            // Nested tuples will require surrounding parentheses
            Parser nested;
            parser_with_flags(self, &nested, self->flags | LOX_PARSE_NO_AUTO_TUPLE);
            lhs = parse_tuple_items(&nested, lhs);
        }
        else {
            break;
        }
        next = T->peek(T);
    }

    // Peek for (binary) operator(s)
    while (next->type > T__OP_MIN && next->type < T__OP_MAX) {
        operator = get_operator_info(next->type);
        if (operator->assoc == ASSOC_LEFT
            && previous && previous->precedence >= operator->precedence
        ) {
            // Don't process this here. Unwind the recursion back to where
            // the precedence is lower and continue building the expression
            // there.
            break;
        }

        // Consume the operator token
        T->next(T);

        // Parse RHS first (before continuing LHS expression)
        rhs = parse_expression_r(self, operator);

        if (operator->operator == T_OP_ASSIGN) {
            lhs = parse_expression_assign(next, lhs, rhs);
        }
        else {
            expr = GC_NEW(ASTExpression);
            parser_node_init((ASTNode*) expr, AST_EXPRESSION, next);

            expr->lhs = lhs;
            expr->binary_op = operator->operator;
            expr->rhs = rhs;

            // Roll the expession forward as the new LHS
            lhs = (ASTNode*) expr;
        }

        next = T->peek(T);
    }

    return lhs;
}

static bool
parse_expr_is_constant(ASTNode *node) {
    assert(node->type == AST_EXPRESSION);
    ASTExpression *expr = (ASTExpression*) node;

    if (expr->lhs->type == AST_EXPRESSION && parse_expr_is_constant(expr->lhs))
        return true;
    if (expr->rhs->type == AST_EXPRESSION && parse_expr_is_constant(expr->rhs))
        return true;
    return expr->lhs->type == AST_LITERAL && expr->rhs->type == AST_LITERAL;
}

static ASTNode*
parse_expression(Parser* self) {
    ASTNode *expr = parse_expression_r(self, 0);

    if (parse_expr_is_constant(expr)) {
        Object *result = LoxEval_EvalAST(expr);
        // XXX: This assumes the size of a literal is less than that of an expr
        expr->type = AST_LITERAL;
        ((ASTLiteral*) expr)->literal = result;
    }

    return expr;
}

static ASTNode*
parse_statement(Parser* self) {
    Token* token = self->tokens->current, *peek, *next;
    ASTNode* result = NULL;

    switch (token->type) {
    // Statement
    case T_VAR: {
        ASTVar* astvar = GC_MALLOC(sizeof(ASTVar));
        parser_node_init((ASTNode*) astvar, AST_VAR, token);
        token = parse_expect(self, T_WORD);
        astvar->name = self->tokens->fetch_text(self->tokens, token);
        astvar->name_length = token->length;

        // Initial value is not required
        peek = self->tokens->peek(self->tokens);
        if (peek->type == T_OP_ASSIGN) {
            parse_expect(self, T_OP_ASSIGN);
            astvar->expression = parse_expression(self);
        }

        result = (ASTNode*) astvar;
        break;
    }
    case T_IF: {
        ASTIf* astif = GC_MALLOC(sizeof(ASTIf));
        parser_node_init((ASTNode*) astif, AST_IF, token);
        parse_expect(self, T_OPEN_PAREN);
        astif->condition = parse_expression(self);
        parse_expect(self, T_CLOSE_PAREN);
        astif->block = parse_statement_or_block(self);

        peek = self->tokens->peek(self->tokens);
        if (peek->type == T_ELSE) {
            parse_expect(self, T_ELSE);
            astif->otherwise = parse_statement_or_block(self);
        }

        result = (ASTNode*) astif;
        break;
    }
    case T_WHILE: {
        ASTWhile* astwhile = GC_MALLOC(sizeof(ASTWhile));
        parser_node_init((ASTNode*) astwhile, AST_WHILE, token);
        parse_expect(self, T_OPEN_PAREN);
        astwhile->condition = parse_expression(self);
        parse_expect(self, T_CLOSE_PAREN);
        astwhile->block = parse_statement_or_block(self);

        result = (ASTNode*) astwhile;
        break;
    }
    case T_FOR: {
        ASTFor* astfor = GC_MALLOC(sizeof(ASTFor));
        parser_node_init((ASTNode*) astfor, AST_FOR, token);
        parse_expect(self, T_OPEN_PAREN);
        astfor->initializer = parse_expression(self);
        parse_expect(self, T_SEMICOLON);
        astfor->condition = parse_expression(self);
        parse_expect(self, T_SEMICOLON);
        astfor->post_loop = parse_expression(self);
        parse_expect(self, T_CLOSE_PAREN);
        astfor->block = parse_statement_or_block(self);

        result = (ASTNode*) astfor;
        break;
    }
    case T_RETURN: {
        ASTReturn* astreturn = GC_MALLOC(sizeof(ASTReturn));
        parser_node_init((ASTNode*) astreturn, AST_RETURN, token);
        // XXX: The return expression is optional?
        astreturn->expression = parse_expression(self);

        result = (ASTNode*) astreturn;
        break;
    }
    case T_CLASS: {
        ASTClass* astclass = GC_NEW(ASTClass);
        parser_node_init((ASTNode*) astclass, AST_CLASS, token);

        next = parse_expect(self, T_WORD);
        astclass->name = (Object*) String_fromCharArrayAndSize(
            self->tokens->fetch_text(self->tokens, next),
            next->length);

        if (self->tokens->peek(self->tokens)->type == T_OP_LT) {
            self->tokens->next(self->tokens);
            astclass->extends = parse_expression(self);
        }

        parse_expect(self, T_OPEN_BRACE);
        while (self->tokens->peek(self->tokens)->type != T_CLOSE_BRACE) {
            next = parse_expect(self, T_WORD);

            ASTFunction* astfun = GC_MALLOC(sizeof(ASTFunction));
            parser_node_init((ASTNode*) astfun, AST_FUNCTION, next);

            // XXX: Use a LoxString for better memory management
            astfun->name = GC_STRNDUP(
                self->tokens->fetch_text(self->tokens, next),
                next->length
            );
            astfun->name_length = next->length;

            astfun->arglist = parse_arg_list(self);
            parse_expect(self, T_CLOSE_PAREN);
            astfun->block = parse_block(self);

            if (astclass->body != NULL)
                astclass->body->next = (ASTNode*) astfun;
            else
                astclass->body = (ASTNode*) astfun;
        }
        parse_expect(self, T_CLOSE_BRACE);
        result = (ASTNode*) astclass;
        break;
    }
    default:
        // This shouldn't happen ...
        result = NULL;
    }

    // Chomp semi-colon, if provided
    peek = self->tokens->peek(self->tokens);
    if (peek->type == T_SEMICOLON)
        self->tokens->next(self->tokens);

    return result;
}

static ASTNode*
parse_statement_or_block(Parser* self) {
    Tokenizer* T = self->tokens;
    Token* token = T->peek(T);

    if (token->type == T_OPEN_BRACE)
        return parse_block(self);
    else
        return parse_next(self);
}

static ASTNode*
parser_parse_next(Parser* self) {
    Token* token = self->tokens->peek(self->tokens);
    ASTNode *rv;

    switch (token->type) {

    // Statement
    case T_VAR:
    case T_IF:
    case T_FOR:
    case T_WHILE:
    case T_RETURN:
    case T_CLASS:
        self->tokens->next(self->tokens);
        rv = parse_statement(self);
        break;

    case T_COMMENT:
        // For now, do nothing -- this isn't the line you're looking for. Move along
        self->tokens->next(self->tokens);
        rv = parser_parse_next(self);
        break;

    case T_EOF:
        return NULL;

    default:
        rv = parse_expression(self);
    }

    if (self->tokens->peek(self->tokens)->type == T_SEMICOLON) {
        // If a semicolon comes at the end of an or statement -- just move over it
        self->tokens->next(self->tokens);
    }
    return rv;
}

ASTNode*
parse_next(Parser* self) {
    return self->previous = parser_parse_next(self);
}

int
parser_init(Parser* parser, Stream* stream) {
    Tokenizer *tokens = tokenizer_init(stream);
    *parser = (Parser) {
        .tokens = tokens,
        .next = parse_next,
    };
    return 0;
}
