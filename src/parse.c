#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    fprintf(stderr, "SyntaxError: line %d, at %d: %s",
        tokens->stream->line, tokens->stream->pos, message);
    ((ASTNode*) NULL)->next->type;
    exit(-1);
}

static Token*
parse_expect(Parser* self, enum token_type type) {
    Tokenizer* T = self->tokens;
    Token* next = T->peek(T);

    if (next->type != type)
        parse_syntax_error(self, "Unexpected token");

    return next;
}

static ASTNode*
parse_CALL(Parser* self) {
    Tokenizer* T = self->tokens;
    Token* func = T->current;
    Token* peek = T->peek(T);
    ASTNode* result;
    ASTNode* args = NULL;
    ASTNode* narg;

    ASTCall* call = calloc(1, sizeof(ASTCall));
    parser_node_init((ASTNode*) call, AST_CALL, func);
    do {
        // Comsume the open paren or the comma
        T->next(T);
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
    Token* next = self->tokens->current;
    switch (next->type) {

    case T_WORD:
        // Peek ahead for '(', which would be function call
        if (self->tokens->peek(self->tokens)->type == T_OPEN_PAREN)
            return parse_CALL(self);
        // Fall through to below
    case T_NUMBER:
    case T_STRING:
    case T_TRUE:
    case T_FALSE:
    case T_NULL: {
        // Do something with the token
        ASTTerm* term = calloc(1, sizeof(ASTTerm));
        parser_node_init((ASTNode*) term, AST_TERM, next);
        term->token_type = next->type;
        term->text = strndup(next->text, next->length);
        return (ASTNode*) term;
        break;
    }

    default:
        parse_syntax_error(self, "Unexpected token type in TERM");
    }
}

static ASTNode*
parse_expression(Parser* self) {
    /* General expression grammar
     * EXPR = UNARY? ( TERM [ OP EXPR ] )
     * TERM = PAREN | CALL | WORD | NUMBER | STRING | TRUE | FALSE | NULL;
     * PAREN = ( EXPR )
     * NUMBER = T_NUMBER
     * STRING = T_STRING
     * OP = T_EQUAL | T_GT | T_GTE | T_LT | T_LTE | T_AND | T_OR
     * UNARY = T_BANG | T_PLUS | T_MINUS
     * CALL = WORD T_OPEN_PAREN [ EXPR ( ',' EXPR )* ] T_CLOSE_PAREN
     */
    Tokenizer* T = self->tokens;
    Token* next = T->next(T);
    ASTNode * result;

    if (next->type == T_BANG || next->type == T_OP_PLUS || next->type == T_OP_MINUS) {
        // Unary operator
        next = T->next(T);
    }

    // parse_TERM
    next = T->peek(T);
    if (next->type == T_OPEN_PAREN) {
        result = parse_expression(self);
        parse_expect(self, T_CLOSE_PAREN);
    }
    else {
        result = parse_TERM(self);
    }

    // peek for OP
    next = T->peek(T);
    if (next->type > T__OP_MIN && next->type < T__OP_MAX) {
        // Push current result as LHS of a binary op
        ASTBinaryOp* binop = calloc(1, sizeof(ASTBinaryOp));
        parser_node_init((ASTNode*) binop, AST_BINARY_OP, next);
        binop->lhs = result;
        binop->op = next->type;
        T->next(T); // Consume the operator token
        binop->rhs = parse_expression(self);
        result = (ASTNode*) binop;
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
        astvar->name = strndup(token->text, token->length);
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

        result = (ASTNode*) astif;
        break;
    }
    case T_FOR:
        parse_expect(self, T_OPEN_PAREN);
        break;

    case T_FUNCTION: {
        ASTFunction* astfun = calloc(1, sizeof(ASTFunction));
        parser_node_init((ASTNode*) astfun , AST_FUNCTION, token);
        token = parse_expect(self, T_WORD);

        parse_expect(self, T_OPEN_PAREN);
        astfun->arglist = parse_arg_list(self);
        parse_expect(self, T_CLOSE_PAREN);
        astfun->block = parse_statement_or_block(self);

        result = (ASTNode*) astfun;
        break;
    }

    default:
        // This shouldn't happen ...
        result = NULL;
    }

    parse_expect(self, T_SEMICOLON);
    return result;
}

static ASTNode*
parse_statement_or_block(Parser* self) {
    Tokenizer* T = self->tokens;
    Token* token = T->peek(T);

    if (token->type == T_OPEN_BRACE)
        return parse_block(self);
    else
        return parse_statement(self);
}

ASTNode*
parse_next(Parser* self) {
    Token* token = self->tokens->next(self->tokens);

    switch (token->type) {
    // Statement
    case T_VAR:
    case T_IF:
    case T_FOR:
    case T_FUNCTION:
    case T_WHILE:
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
