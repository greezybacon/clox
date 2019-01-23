#include <stdio.h>
#include <stdlib.h>

#include "token.h"

static struct token_description {
    enum token_type     type;
    char*               description;
}
TokenDescriptions[] = {
    { T_EOF, "T_EOF" },
    { T_WHITESPACE, "T_WHITESPACE" },
    { T_TRUE, "T_TRUE" },
    { T_FALSE, "T_FALSE" },
    { T_NUMBER, "T_NUMBER" },
    { T_STRING, "T_STRING" },
    { T_WORD, "T_WORD" },
    { T_NULL, "T_NULL" },
    { T_OP_PLUS, "T_OP_PLUS" },
    { T_OP_MINUS, "T_OP_MINUS" },
    { T_OP_STAR, "T_OP_STAR" },
    { T_OP_SLASH, "T_OP_SLASH" },
    { T_OP_LT, "T_OP_LT" },
    { T_OP_LTE, "T_OP_LTE" },
    { T_OP_GT, "T_OP_GT" },
    { T_OP_GTE, "T_OP_GTE" },
    { T_OP_EQUAL, "T_OP_EQUAL" },
    { T_OP_ASSIGN, "T_OP_ASSIGN" },
    { T_OP_IN, "T_OP_IN" },
    { T_BANG, "T_BANG" },
    { T_AND, "T_AND" },
    { T_OR, "T_OR" },
    { T_OPEN_PAREN, "T_OPEN_PAREN" },
    { T_CLOSE_PAREN, "T_CLOSE_PAREN" },
    { T_VAR, "T_VAR" },
    { T_IF, "T_IF" },
    { T_ELSE, "T_ELSE" },
    { T_WHILE, "T_WHILE" },
    { T_FOR, "T_FOR" },
    { T_FUNCTION, "T_FUNCTION" },
    { T_CLASS, "T_CLASS" },
    { T_DOT, "T_DOT" },
    { T_OPEN_PAREN, "T_OPEN_PAREN" },
    { T_CLOSE_PAREN, "T_CLOSE_PAREN" },
    { T_SEMICOLON, "T_SEMICOLON" },
    { T_OPEN_BRACE, "T_OPEN_BRACE" },
    { T_CLOSE_BRACE, "T_CLOSE_BRACE" },
    { T_OPEN_BRACKET, "T_OPEN_BRACKET" },
    { T_CLOSE_BRACKET, "T_CLOSE_BRACKET" },
    { T_COMMA, "T_COMMA" },
    { T_RETURN, "T_RETURN" },
    { T_COMMENT, "T_COMMENT" },
};
static const int cTokenDescriptions = sizeof(TokenDescriptions) / sizeof(TokenDescriptions[0]);

static struct operator_description {
    enum token_type     type;
    char*               description;
}
OperatorChars[] = {
    { T_OP_PLUS, "+" },
    { T_OP_MINUS, "-" },
    { T_OP_STAR, "*" },
    { T_OP_SLASH, "/" },
    { T_OP_LT, "<" },
    { T_OP_LTE, "<=" },
    { T_OP_GT, ">" },
    { T_OP_GTE, ">=" },
    { T_OP_EQUAL, "==" },
    { T_OP_ASSIGN, "=" },
    { T_OP_IN, "in" },
    { T_BANG, "!" },
    { T_AND, "and" },
    { T_OR, "or" },
    { T_DOT, "." },
};
static const int cOperatorChars = sizeof(OperatorChars) / sizeof(OperatorChars[0]);

static int
compare_token_desc(const void *a, const void *b) {
    return ((struct token_description*)a)->type - ((struct token_description*)b)->type;
}
    
void
print_token(FILE* output, Token* token) {
    struct token_description* T, key = { .type = token->type };
    T = bsearch(&key, TokenDescriptions, cTokenDescriptions, 
        sizeof(struct token_description), compare_token_desc);
    if (T == NULL) {
        fprintf(output, "%d: No such token type", token->type);
        return;
    }
    fprintf(output, "{%s (%d:%d)}", T->description, token->line, token->pos);
}

void
print_token2(FILE* output, Token* token) {
    print_token(output, token);
    fprintf(output, ":'%.*s'", token->length, token->text);
}

char*
get_token_type(enum token_type type) {
    struct token_description* T, key = { .type = type };
    T = bsearch(&key, TokenDescriptions, cTokenDescriptions, 
        sizeof(struct token_description), compare_token_desc);
    if (T == NULL)
        return NULL;
    return T->description;
}

char*
get_operator(enum token_type type) {
    struct operator_description* T, key = { .type = type };
    T = bsearch(&key, OperatorChars, cOperatorChars, 
        sizeof(struct operator_description), compare_token_desc);
    if (T == NULL) {
        return NULL;
    }
    return T->description;
}