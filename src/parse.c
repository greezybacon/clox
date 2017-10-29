#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_token.h"
#include "parse.h"

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
    
    fprintf(stderr, "SyntaxError: line %d, at %d: %s\n",
        tokens->stream->line, tokens->stream->pos, message);
    exit(-1);
}

static Token*
parse_expect(Parser* self, enum token_type type) {
    Tokenizer* T = self->tokens;
    Token* next = T->next(T);

    if (next->type != type) {
        char buffer[255];
        snprintf(buffer, sizeof(buffer), "Expected %d, got %.*s", type, next->length,
            next->text);
        parse_syntax_error(self, buffer);
    }

    return next;
}

static ASTNode*
parse_CALL(Parser* self, Token* term) {
    Tokenizer* T = self->tokens;
    Token* func = T->current;
    Token* peek = T->peek(T);
    ASTNode* result;
    ASTNode* args = NULL;
    ASTNode* narg;

    ASTCall* call = calloc(1, sizeof(ASTCall));
    parser_node_init((ASTNode*) call, AST_CALL, func);
    call->function_name = term->text;
    call->name_length = term->length;
    fprintf(stdout, "CALL: %.*s\n", term->length, term->text);

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

static ASTNode*
parse_TERM(Parser* self) {
    Tokenizer* T = self->tokens;
    Token* next = self->tokens->current;
    ASTTerm* term = NULL;

    switch (next->type) {
    case T_OPEN_PAREN:
        // parse_TERM -- PARENthensized TERM
        if (T->peek(T)->type != T_CLOSE_PAREN) {
            term = (ASTExpressionChain*) parse_expression(self);
        }
        parse_expect(self, T_CLOSE_PAREN);
        break;

    case T_WORD: {
        // Peek ahead for '(', which would be function call
        Token previous = *next;
        if (self->tokens->peek(self->tokens)->type == T_OPEN_PAREN)
            return parse_CALL(self, &previous);
        // Fall through to below
    }
    case T_NUMBER:
    case T_STRING:
    case T_TRUE:
    case T_FALSE:
    case T_NULL:
        // Do something with the token
        term = calloc(1, sizeof(ASTTerm));
        parser_node_init((ASTNode*) term, AST_TERM, next);
        term->token_type = next->type;
        term->text = self->tokens->fetch_text(self->tokens, next);
        term->length = next->length;
        break;

    default: {
        char buffer[255];
        snprintf(buffer, sizeof(buffer), "Unexpected token type in TERM, got %s", 
            get_token_type(next->type));
        parse_syntax_error(self, buffer);
    }}
    
    if (term && next->type == T_NUMBER) {
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
    return (ASTNode*) term;
}

static ASTNode*
parse_expression(Parser* self) {
    /* General expression grammar
     * EXPR = UNARY? ( TERM [ OP EXPR ] )
     * TERM = PAREN | CALL | WORD | NUMBER | STRING | TRUE | FALSE | NULL
     * PAREN = "(" EXPR ")"
     * NUMBER = T_NUMBER
     * STRING = T_STRING
     * OP = T_EQUAL | T_GT | T_GTE | T_LT | T_LTE | T_AND | T_OR
     * UNARY = T_BANG | T_PLUS | T_MINUS
     * CALL = WORD T_OPEN_PAREN [ EXPR ( ',' EXPR )* ] T_CLOSE_PAREN
     */
    Tokenizer* T = self->tokens;
    Token* next = T->next(T);
    ASTNode * result;
    ASTExpressionChain *expr, *expr_next=NULL;
    
    // Otherwise, expect a simple TERM
    
    expr = calloc(1, sizeof(ASTExpressionChain));
    parser_node_init((ASTNode*) expr, AST_EXPRESSION_CHAIN, next);
    result = (ASTNode*) expr;
    
    for (;;) {
        // Capture unary operator
        if (next->type == T_BANG || next->type == T_OP_PLUS || next->type == T_OP_MINUS) {
            expr->unary_op = next->type;
            T->next(T);
        }
        
        expr->term = parse_TERM(self);
        //expr->prev = NULL;

        // Peek for (binary) operator
        next = T->peek(T);
        if (next->type < T__OP_MIN || next->type > T__OP_MAX)
            break;

        expr_next = calloc(1, sizeof(ASTExpressionChain));
        parser_node_init((ASTNode*) expr_next, AST_EXPRESSION_CHAIN, next);

        T->next(T); // Consume the operator token
        expr->op = next->type;
        expr = ((ASTNode*) expr)->next = expr_next;
        
        // Continue to the following token
        next = T->next(T);
    }
    return result;
}

static ASTNode*
parse_arg_list(Parser* self) {
    Tokenizer* T = self->tokens;
    Token* peek;
    ASTNode *result, *narg;
    ASTNode *args = NULL;

    parse_expect(self, T_OPEN_PAREN);
    for (;;) {
        peek = T->peek(T);
        if (peek->type == T_CLOSE_PAREN) {
            break;
        }
        else if (peek->type == T_COMMA) {
            T->next(T);
        }
        // XXX: This won't work. Need a reference to the args list, the
        // current arg, and the previous arg?
        narg = parse_expression(self);
        if (args == NULL) {
            result = args = narg;
        }
        else {
            args = args->next = narg;
        }
    }
    return result;
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

    return result;
}

static ASTNode*
parse_statement(Parser* self) {
    Token* token = self->tokens->current;
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

        result = (ASTNode*) astif;
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
    case T_FUNCTION: {
        ASTFunction* astfun = calloc(1, sizeof(ASTFunction));
        parser_node_init((ASTNode*) astfun , AST_FUNCTION, token);
        token = parse_expect(self, T_WORD);

        parse_expect(self, T_OPEN_PAREN);
        astfun->arglist = parse_arg_list(self);
        parse_expect(self, T_CLOSE_PAREN);
        astfun->block = parse_block(self);

        result = (ASTNode*) astfun;
        break;
    }

    default:
        // This shouldn't happen ...
        result = NULL;
    }

    // Chomp semi-colon, if provided
    Token* peek = self->tokens->peek(self->tokens);
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

ASTNode*
parse_next(Parser* self) {
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
    case T_FUNCTION:
    case T_WHILE:
        self->tokens->next(self->tokens);
        return parse_statement(self);

    case T_EOF:
        return NULL;

    default:
        return parse_expression(self);
    }
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
