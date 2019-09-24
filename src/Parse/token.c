#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "stream.h"
#include "debug_token.h"

#include "Vendor/bdwgc/include/gc.h"

static Token previous, next;

static char
peek_char(Tokenizer *self) {
    char c = self->stream->peek(self->stream);
    return c;
}

static char
next_char(Tokenizer *self) {
    char c = self->stream->next(self->stream);
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

    signed char c, begin;

    // Ignore whitespace
    do {
        c = next_char(self);
    }
    while (isspace(c) && c != -1);

    previous = next;
    self->previous = &previous;

    Token *token = &next;
    *token = (struct token) {
        .pos = self->stream->offset,
        .line = self->stream->line,
        .stream_pos = self->stream->pos - 1,
        .type = T_EOF,
        .length = 0,
        .text = NULL,
    };

    self->current = token;

    switch (c) {
    // Strings
    case '\"':
    case '\'':
        // Read to ending char
        begin = c;
        // Ignore opening char
        token->stream_pos++;

        do {
            c = next_char(self);
            if (c == '\\')
                c = next_char(self);
        }
        while (c != begin && c > 0);
        token->type = T_STRING;
        token->text = self->fetch_text(self, token);
        // Ignore closing char
        token->length--;
        break;

    // Operators
    case '+':
        token->type = T_OP_PLUS;
        break;

    case '-':
        // This one can be tricky. If it's followed by an operator, then it's
        // always an operator. If it's preceeded by an operator, than it's
        // always a unary negative.
        if (!(self->previous->type > T__OP_MIN) || !(self->previous->type < T__OP_MAX))
            // It's an operator
            token->type = T_OP_MINUS;
        else if (!isdigit(peek_char(self)))
            // It's an operator
            token->type = T_OP_MINUS;
        break;

    case '*':
        token->type = T_OP_STAR;
        break;

    case '/':
        if (peek_char(self) == '/') {
            // Scan to end of line (comment)
            while (next_char(self) != '\n');
            token->type = T_COMMENT;
        }
        else {
            token->type = T_OP_SLASH;
        }
        break;

    case '!':
        if (peek_char(self) == '=') {
            next_char(self);
            token->type = T_OP_NOTEQUAL;
        }
        else {
            token->type = T_BANG;
        }
        break;

    case ':':
        token->type = T_COLON;
        break;

    case '>':
        if (peek_char(self) == '=') {
            next_char(self);
            token->type = T_OP_GTE;
        }
        else if (peek_char(self) == '>') {
            next_char(self);
            token->type = T_OP_RSHIFT;
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
        else if (peek_char(self) == '<') {
            next_char(self);
            token->type = T_OP_LSHIFT;
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

    case '%':
        token->type = T_OP_PERCENT;
        break;

    case '&':
        token->type = T_OP_AMPERSAND;
        break;

    case '|':
        token->type = T_OP_PIPE;
        break;

    case '^':
        token->type = T_OP_CARET;
        break;

    case '~':
        token->type = T_OP_TILDE;
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
        
    case '[':
        token->type = T_OPEN_BRACKET;
        break;

    case ']':
        token->type = T_CLOSE_BRACKET;
        break;

    case ',':
        token->type = T_COMMA;
        break;
    }

    if (token->type != T_EOF)
        return token;

    // Words (start with a letter or _, allow _ and digits thereafter)
    if (isalpha(c) || c == '_') {
        while (isalnum(peek_char(self)) || peek_char(self) == '_')
            next_char(self);

        const char* token_text = self->fetch_text(self, token);
        switch (token->length) {
        case 2:
            if (0 == strncmp(token_text, "if", 2))
                token->type = T_IF;
            else if (0 == strncasecmp(token_text, "or", 2))
                token->type = T_OR;
            else if (0 == strncmp(token_text, "in", 2))
                token->type = T_OP_IN;
            break;
        case 3:
            if (0 == strncmp(token_text, "for", 3))
                token->type = T_FOR;
            else if (0 == strncasecmp(token_text, "fun", 3))
                token->type = T_FUNCTION;
            else if (0 == strncmp(token_text, "var", 3))
                token->type = T_VAR;
            else if (0 == strncasecmp(token_text, "and", 3))
                token->type = T_AND;
            break;
        case 4:
            if (0 == strncmp(token_text, "else", 4))
                token->type = T_ELSE;
            else if (0 == strncasecmp(token_text, "null", 4))
                token->type = T_NULL;
            else if (0 == strncasecmp(token_text, "true", 4))
                token->type = T_TRUE;
            else if (0 == strncasecmp(token_text, "this", 4))
                token->type = T_THIS;
            break;
        case 5:
            if (0 == strncasecmp(token_text, "false", 5))
                token->type = T_FALSE;
            else if (0 == strncmp(token_text, "while", 5))
                token->type = T_WHILE;
            else if (0 == strncmp(token_text, "class", 5))
                token->type = T_CLASS;
            else if (0 == strncmp(token_text, "super", 5))
                token->type = T_SUPER;
            else if (0 == strncmp(token_text, "break", 5))
                token->type = T_BREAK;
            break;
        case 6:
            if (0 == strncmp(token_text, "return", 6))
                token->type = T_RETURN;
            break;
        case 7:
            if (0 == strncmp(token_text, "foreach", 7))
                token->type = T_FOREACH;
            break;
        case 8:
            if (0 == strncmp(token_text, "function", 8))
                token->type = T_FUNCTION;
            else if (0 == strncmp(token_text, "continue", 8))
                token->type = T_CONTINUE;
        }

        // Otherwise its a plain old word
        if (token->type == T_EOF) {
            token->type = T_WORD;
        }
    }
    // Number
    else if (c == '-' || c == '.' || isdigit(c)) {
        for (;;) {
            c = peek_char(self);
            if (c == 0 || !(c == '.' || isdigit(c)))
                break;
            next_char(self);
        }
        token->type = T_NUMBER;
    }

    if (!token->length)
        token->length = self->stream->pos - token->stream_pos;
    token->text = self->fetch_text(self, token);

    return token;
}

static Token*
peek_token(Tokenizer *self) {
    if (self->_peek == NULL)
        self->_peek = next_token(self);

    return self->_peek;
}

static const char*
read_from_token(Tokenizer *self, Token* token) {
    if (!token->length)
        token->length = self->stream->pos - token->stream_pos;
    // NOTE: read() may return a malloc()d pointer ...
    if (!token->text)
        token->text = self->stream->read(self->stream, token->stream_pos, token->length);
    return token->text;
}

static Token*
debug_next_token(Tokenizer *self) {
    Token *next = next_token(self);
    print_token(stderr, next);
    return next;
}

Tokenizer*
tokenizer_init(Stream* stream) {
    Tokenizer * self = GC_MALLOC(sizeof(Tokenizer));
    *self = (Tokenizer) {
        .stream = stream,
        .next = next_token,
        .peek = peek_token,
        .fetch_text = read_from_token,
    };
    return self;
}
