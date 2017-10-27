#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

#include "token.h"
#include "stream.h"

static Token previous;
static Token current;
static Token next;

static char
peek_char(Tokenizer *self) {
    char c = self->stream->peek(self->stream);
    if (c == -1)
        return 0;

    return c;
}

static char
next_char(Tokenizer *self) {
    char c = self->stream->next(self->stream);
    if (c == -1)
        return 0;

    return c;
}

static Token*
next_token(Tokenizer *self) {
    // If next is stashed from peek() call, then return the stashed token
    if (self->_peek != NULL) {
        Token* T = self->_peek;
        self->_peek = NULL;
        return T;
    }

    char c, begin;

    // Ignore whitespace
    do {
        c = next_char(self);
    }
    while (isspace(c) && c != 0);

    Token *token = &next;
    int start = self->stream->pos;
    *token = (struct token) {
        .pos = self->stream->offset, 
        .line = self->stream->line,
        .type = T_EOF,
    };

    switch (c) {
    // Strings
    case '\"':
    case '\'':
        // Read to ending char
        begin = c;
        do {
            c = next_char(self);
            if (c == '\\')
                c = next_char(self);
        }
        while (c != begin && c != 0);
        token->type = T_STRING;
        break;

    // Operators
    case '+':
        token->type = T_OP_PLUS;
        break;

    case '-':
        token->type = T_OP_MINUS;
        break;

    case '*':
        token->type = T_OP_STAR;
        break;

    case '/':
        token->type = T_OP_SLASH;
        break;

    case '!':
        token->type = T_BANG;
        break;

    case '>':
        if (peek_char(self) == '=') {
            next_char(self);
            token->type = T_OP_GTE;
        }
        else {
            token->type = T_OP_GT;
        }
        break;

    case '<':
        if (peek_char(self) == '=') {
            next_char(self);
            token->type = T_OP_LTE;
        }
        else {
            token->type = T_OP_LT;
        }
        break;

    case '=':
        if (peek_char(self) == '=') {
            next_char(self);
            token->type = T_OP_EQUAL;
        }
        else {
            token->type = T_OP_ASSIGN;
        }
        break;

    case '.':
        if (isdigit(peek_char(self)))
            break;
        token->type = T_DOT;
        break;

    // Control chars
    case ';':
        token->type = T_SEMICOLON;
        break;

    case '(':
        token->type = T_OPEN_PAREN;
        break;

    case ')':
        token->type = T_CLOSE_PAREN;
        break;

    case '{':
        token->type = T_OPEN_BRACE;
        break;

    case '}':
        token->type = T_CLOSE_BRACE;
        break;

    case ',':
        token->type = T_COMMA;
        break;
    }

    if (token->type != T_EOF)
        return token;

    // Words
    if (isalpha(c)) {
        while (isalnum(peek_char(self)))
            next_char(self);

        token->length = start - self->stream->pos;
        if (token->length == 2
            && strncasecmp(token->text, "if", 2)
        ) {
            token->type = T_IF;
        }
        else if (token->length == 3
            && strncasecmp(token->text, "for", 3)
        ) {
            token->type = T_FOR;
        }
        else if (token->length == 3
            && strncasecmp(token->text, "var", 3)
        ) {
            token->type = T_VAR;
        }
        else if (token->length == 4
            && strncasecmp(token->text, "else", 4)
        ) {
            token->type = T_ELSE;
        }
        else if (token->length == 4
            && strncasecmp(token->text, "null", 4)
        ) {
            token->type = T_NULL;
        }
        else if (token->length == 4
            && strncasecmp(token->text, "true", 4)
        ) {
            token->type = T_TRUE;
        }
        else if (token->length == 5
            && strncasecmp(token->text, "false", 5)
        ) {
            token->type = T_FALSE;            
        }
        else if (token->length == 5
            && strncasecmp(token->text, "while", 5)
        ) {
            token->type = T_FALSE;            
        }
        else if (token->length == 8
            && strncasecmp(token->text, "function", 8)
        ) {
            token->type = T_FUNCTION;
        }
        else {
            token->type = T_WORD;
        }
    }
    // Number
    else if (c == '.' || isdigit(c)) {
        for (;;) {
            c = peek_char(self);
            if (c == 0 || c != '.' || !isdigit(c))
                break;
            next_char(self);
        }
        token->type = T_NUMBER;
    }

    token->length = self->stream->pos - start;
    token->text = calloc(1, token->length);
    self->stream->read(self->stream, start, token->text, token->length);
    self->current = token;
    return token;
}

static Token*
peek_token(Tokenizer *self) {
    if (self->_peek == NULL)
        self->_peek = next_token(self);

    return self->_peek;
}

Tokenizer*
tokenizer_init(Stream* stream) {
    Tokenizer * self = calloc(1, sizeof(Tokenizer));
    *self = (Tokenizer) {
        .stream = stream,
        .next = next_token,
        .peek = peek_token,
    };
    return self;
}
