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
parse_syntax_error(Parser* self) {
    Tokenizer* tokens = self->tokens;
    fprintf(stderr, "SyntaxError: line %d, at %d",
        tokens->line, tokens->pos);
}

static Token*
parse_expect(Parser* self, enum token_type type) {
    Tokenizer* T = self->tokens;
    Token* next = T->peek(T);

    if (next->type != type)
        parse_syntax_error(self);

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

    result = (ASTNode*) call;
    *result = (ASTNode) {
        .type = AST_CALL,
        .line = func->line,
        .offset = func->pos,
    };

    parse_expect(self, T_CLOSE_PAREN);

    return result;
}

static ASTNode*
parse_TERM(Parser* self) {
    Token* next = self->tokens->current;
    ASTNode *result;
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
        result = (ASTNode*) term;
        *result = (ASTNode) {
            .type = AST_TERM,
            .line = next->line,
            .offset = next->pos,
        };
        term->token_type = next->type;
        term->text = strndup(next->text, next->length);
        break;
    }

    default:
        parse_syntax_error(self);
    }

    return result;
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
    if (next->type >= T_OP_PLUS && next->type <= T_OP_EQUAL) {
        // Push current result as LHS of a binary op
        ASTBinaryOp* result2 = calloc(1, sizeof(ASTBinaryOp));
        result = (ASTNode*) result2;
        *result = (ASTNode) {
            .type = AST_BINARY_OP,
            .line = next->line,
            .offset = next->pos,
        };
        result2->lhs = result;
        result2->op = next->type;
        T->next(T); // Consume the operator token
        result2->rhs = parse_expression(self);
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
        result = (ASTNode*) astvar;
        *result = (ASTNode) {
            .type = AST_VAR,
            .line = token->line,
            .offset = token->pos,
        };
        token = parse_expect(self, T_WORD);
        astvar->name = strndup(token->text, token->length);
        parse_expect(self, T_OP_ASSIGN);
        astvar->expression = parse_expression(self);
        break;
    }
    case T_IF: {
        ASTIf* astif = calloc(1, sizeof(ASTIf));
        result = (ASTNode*) astif;
        *result = (ASTNode) {
            .type = AST_IF,
            .line = token->line,
            .offset = token->pos,
        };
        parse_expect(self, T_OPEN_PAREN);
        astif->condition = parse_expression(self);
        parse_expect(self, T_CLOSE_PAREN);

        result = (void*) astif;
        break;
    }
    case T_FOR:
        parse_expect(self, T_OPEN_PAREN);
        break;

    case T_FUNCTION: {
        ASTFunction* astfun = calloc(1, sizeof(ASTFunction));
        result = (ASTNode*) astfun;
        *result = (ASTNode) {
            .type = AST_FUNCTION,
            .line = token->line,
            .offset = token->pos,
        };
        token = parse_expect(self, T_WORD);

        parse_expect(self, T_OPEN_PAREN);
        astfun->arglist = parse_arg_list(self);
        parse_expect(self, T_CLOSE_PAREN);
        astfun->block = parse_statement_or_block(self);
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
parser_init(Parser* parser, const char * stream, int length) {
    Tokenizer *tokens = tokenizer_init(stream, length);
    *parser = (Parser) {
        .tokens = tokens,
        .next = parse_next,
    };
    return 0;
}
