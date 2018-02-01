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
        .next = NULL,
    };
}

static void
parse_syntax_error(Parser* self, char* message) {
    Tokenizer* tokens = self->tokens;
    Token* next = tokens->peek(tokens);
    
    fprintf(stderr, "Parse Error: line %d, at %d: %s\n",
        tokens->stream->line, tokens->stream->pos, message);
    exit(-1);
}

static Token*
parse_expect(Parser* self, enum token_type type) {
    Tokenizer* T = self->tokens;
    Token* next = T->next(T);

    if (next->type != type) {
        char buffer[255];
        snprintf(buffer, sizeof(buffer), "Expected %s(%d), got %.*s",
            get_token_type(type), type, next->length, next->text);
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
    while (peek->type != T_CLOSE_BRACE);
    
    // Console close brace
    T->next(T);

    return result;
}

static ASTNode*
parse_slice(Parser* self, ASTNode* object) {
    return object;
}

static ASTNode*
parse_invoke(Parser* self, ASTNode* callable) {
    Tokenizer* T = self->tokens;
    Token* func = T->current;
    Token* peek = T->peek(T);
    ASTNode* result;
    ASTNode* args = NULL;
    ASTNode* narg;

    ASTInvoke* call = calloc(1, sizeof(ASTInvoke));
    parser_node_init((ASTNode*) call, AST_INVOKE, func);
    call->callable = callable;

    do {
        // Comsume the open paren or the comma
        T->next(T);

        // Arguments are optional ...
        if (T->peek(T)->type == T_CLOSE_PAREN)
            break;

        narg = parse_expression(self);
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

        param = calloc(1, sizeof(ASTFuncParam));
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

static ASTNode*
parse_TERM(Parser* self) {
    Tokenizer* T = self->tokens;
    Token* next = self->tokens->current;
    ASTTerm* term = NULL;
    ASTNode* result;
    bool is_literal = false;

    switch (next->type) {
    case T_OPEN_PAREN:
        // XXX: This indicates CALL if it is not the first token in the
        // expression or if it is not preceeded by an operator

        // parse_TERM -- PARENthensized TERM
        if (T->peek(T)->type != T_CLOSE_PAREN) {
            result = (ASTNode*) parse_expression(self);
        }
        parse_expect(self, T_CLOSE_PAREN);
        break;

    case T_NUMBER:
    case T_STRING:
    case T_TRUE:
    case T_FALSE:
    case T_NULL:
        is_literal = true;
    case T_WORD:
        // Do something with the token
        term = calloc(1, sizeof(ASTTerm));
        parser_node_init((ASTNode*) term, AST_TERM, next);
        term->token_type = next->type;
        term->text = self->tokens->fetch_text(self->tokens, next);
        term->length = next->length;
        result = (ASTNode*) term;

        if (next->type == T_NUMBER) {
            char* endpos;
            if (memchr(term->text, '.', next->length) != NULL) {
                term->token.real = strtold(term->text, &endpos);
                if (term->token.real == 0.0 && endpos == term->text) {
                    parse_syntax_error(self, "Invalid floating point number");
                }
                term->isreal = 1;
            }
            else {
                term->token.integer = strtoll(term->text, &endpos, 10);
                if (term->token.integer == 0 && endpos == term->text) {
                    parse_syntax_error(self, "Invalid integer number");
                }
            }
        }

        if (is_literal) {
            Object *literal = eval_term(NULL, term);
            free(term);
            result = calloc(1, sizeof(ASTLiteral));
            parser_node_init((ASTNode*) result, AST_LITERAL, next);
            ((ASTLiteral*) result)->literal = literal;
        }
        break;

    case T_FUNCTION: {
        ASTFunction* astfun = calloc(1, sizeof(ASTFunction));
        parser_node_init((ASTNode*) astfun, AST_FUNCTION, next);

        if ((next = T->peek(T))->type == T_WORD) {
            T->next(T);
            astfun->name = strndup(
                self->tokens->fetch_text(self->tokens, next),
                next->length
            );
        }

        astfun->arglist = parse_arg_list(self);
        parse_expect(self, T_CLOSE_PAREN);
        astfun->block = parse_block(self);

        result = (ASTNode*) astfun;
        break;
    }
    default: {
        char buffer[255];
        snprintf(buffer, sizeof(buffer), "Unexpected token type in TERM, got %s", 
            get_token_type(next->type));
        parse_syntax_error(self, buffer);
    }}

    // Read ahead for invoke or slice
    for (;;) {
        next = T->peek(T);
        if (next->type == T_OPEN_PAREN) {
            result = parse_invoke(self, result);
        }
        else if (next->type == T_OPEN_BRACKET) {
            result = parse_slice(self, result);
        }
        else {
            break;
        }
    }

    return (ASTNode*) result;
}

typedef struct stack_thingy {
    unsigned            index;
    void**              head[16];
} Stack;

static void*
stack_pop(Stack* self) {
    assert(self->index > 0);
    return *(self->head + --self->index);
}

static void*
stack_peek(Stack* self) {
    if (self->index == 0)
        return NULL;

    return *(self->head + self->index - 1);
}

static void
stack_push(Stack* self, void* object) {
    assert(self->index < 15);
    
    // TODO: Resize stack on items larger than 15
    
    *(self->head + self->index++) = object;
}

static unsigned
stack_depth(Stack* self) {
    return self->index;
}

static void
stack_init(Stack* stack) {
    *stack = (Stack) {
        .index = 0,
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
    fprintf(stdout, ">Huh %d\n", op);
    return 0;
}

static Object*
parse_word2string(ASTTerm* term) {
    assert(term->token_type == T_WORD);
    Object* rv = (Object*) String_fromCharArrayAndSize(term->text, term->length);
    printf("%p\n", rv);
    return rv;
}

static inline ASTNode*
parse_expression_binop(Token* token, Stack* stack, int binop) {
    if (binop == T_OP_ASSIGN) {
        ASTAssignment* assign = calloc(1, sizeof(ASTAssignment));
        parser_node_init((ASTNode*) assign, AST_ASSIGNMENT, token);
        assign->expression = (ASTNode*) stack_pop(stack);
        assign->name = parse_word2string(stack_pop(stack));
        return (ASTNode*) assign;
    }

    ASTExpression* expr = calloc(1, sizeof(ASTExpression));
    parser_node_init((ASTNode*) expr, AST_EXPRESSION, token);
    expr->rhs = (ASTNode*) stack_pop(stack);
    expr->lhs = (ASTNode*) stack_pop(stack);
    expr->binary_op = binop;
    return (ASTNode*) expr;
}

static ASTNode*
parse_expression(Parser* self) {
    /* General expression grammar
     * EXPR = UNARY? COMPONENT [ OP EXPR ]
     * COMPONENT = TERM [ CALL ]*
     * TERM = PAREN | FUNCTION | WORD | NUMBER | STRING | TRUE | FALSE | NULL
     * CALL = INVOKE | SLICE
     * PAREN = "(" EXPR ")"
     * OP = T_EQUAL | T_GT | T_GTE | T_LT | T_LTE | T_AND | T_OR
     * UNARY = T_BANG | T_PLUS | T_MINUS
     * FUNCTION = "fun" [ WORD ] "(" [ EXPR ( "," EXPR )* ] ")" BLOCK
     * INVOKE = T_OPEN_PAREN [ EXPR ( ',' EXPR )* ] T_CLOSE_PAREN
     * SLICE = T_OPEN_BRACKET [ EXPR [ ':' EXPR [ ':' EXPR ] ] ] T_CLOSE_BRACKET
     */
    Tokenizer* T = self->tokens;
    Token* next = T->next(T), *prev;
    int precedence;
    ASTNode * result;
    ASTExpression *expr;

    Stack _values, *values = &_values;
    stack_init(values);
    enum token_type operators[16], binop, *prevop = operators;
    operators[0] = 0;

    for (;;) {
        // Capture unary operator
        if (next->type == T_BANG || next->type == T_OP_PLUS || next->type == T_OP_MINUS) {
            expr->unary_op = next->type;
            T->next(T);
        }

        stack_push(values, parse_TERM(self));

        // Peek for (binary) operator
        next = T->peek(T);
        if (next->type < T__OP_MIN || next->type > T__OP_MAX)
            break;

        precedence = get_precedence(next->type);
        for (;;) {
            binop = *prevop;
            if (!binop || get_precedence(binop) < precedence)
                break;

            stack_push(values, parse_expression_binop(next, values, *prevop--));
        }

        *++prevop = next->type;
        T->next(T); // Consume the operator token

        // Continue to the following token
        next = T->next(T);
    }

    // Empty the operator stack
    while (*prevop > 0) {
        stack_push(values, parse_expression_binop(next, values, *prevop--));
    }

    result = (ASTNode*) stack_pop(values);
    assert(values->index == 0);

    return result;
}

static ASTNode*
parse_statement(Parser* self) {
    Token* token = self->tokens->current, *peek;
    ASTNode* result = NULL;

    switch (token->type) {
    // Statement
    case T_VAR: {
        ASTVar* astvar = calloc(1, sizeof(ASTVar));
        parser_node_init((ASTNode*) astvar, AST_VAR, token);
        token = parse_expect(self, T_WORD);
        astvar->name = self->tokens->fetch_text(self->tokens, token);
        parse_expect(self, T_OP_ASSIGN);
        astvar->expression = parse_expression(self);

        result = (ASTNode*) astvar;
        break;
    }
    case T_IF: {
        ASTIf* astif = calloc(1, sizeof(ASTIf));
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
        ASTWhile* astwhile = calloc(1, sizeof(ASTWhile));
        parser_node_init((ASTNode*) astwhile, AST_WHILE, token);
        parse_expect(self, T_OPEN_PAREN);
        astwhile->condition = parse_expression(self);
        parse_expect(self, T_CLOSE_PAREN);
        astwhile->block = parse_statement_or_block(self);

        result = (ASTNode*) astwhile;
        break;
    }
    case T_FOR: {
        ASTFor* astfor = calloc(1, sizeof(ASTFor));
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
        ASTExpression* expr = parse_expression(self);
        // expr->isreturn = 1;

        result = (ASTNode*) expr;
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

    switch (token->type) {
    // statement separator
    case T_SEMICOLON:
        self->tokens->next(self->tokens);
        return parse_next(self);

    // Statement
    case T_VAR:
    case T_IF:
    case T_FOR:
    case T_WHILE:
    case T_RETURN:
        self->tokens->next(self->tokens);
        return parse_statement(self);

    case T_EOF:
        return NULL;

    default:
        return parse_expression(self);
    }
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
