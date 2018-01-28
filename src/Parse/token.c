#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

#include "token.h"
#include "stream.h"
#include "debug_token.h"

static Token next = { 0 };

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
        do {
            c = next_char(self);
            if (c == '\\')
                c = next_char(self);
        }
        while (c != begin && c != 0);
        token->type = T_STRING;
        token->text = self->fetch_text(self, token);
        break;

    // Operators
    case '+':
        token->type = T_OP_PLUS;
        break;

    case '-':
        if (!isdigit(peek_char(self)))
            token->type = T_OP_MINUS;
        break;

    case '*':
        token->type = T_OP_STAR;
        break;

    case '/':
        if (peek_char(self) == '/') {
            // Scan to end of line (comment)
            while (next_char(self) != '\n');
        }
        else {
            token->type = T_OP_SLASH;
        }
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
        while (isalnum(peek_char(self)))
            next_char(self);

        char* token_text = self->fetch_text(self, token);
        switch (token->length) {
        case 2:
            if (0 == strncmp(token_text, "if", 2))
                token->type = T_IF;
            else if (0 == strncasecmp(token_text, "or", 2))
                token->type = T_OR;
            break;
        case 3:
            if (0 == strncmp(token_text, "for", 3))
                token->type = T_FOR;
            else if (0 == strncmp(token_text, "var", 3))
                token->type = T_VAR;
            else if (0 == strncasecmp(token_text, "and", 3))
                token->type = T_AND;
            break;
        case 4:
            if (0 == strncmp(token_text, "else", 4))
                token->type = T_ELSE;
            else if (0 == strncmp(token_text, "null", 4))
                token->type = T_NULL;
            else if (0 == strncmp(token_text, "true", 4))
                token->type = T_TRUE;
            break;
        case 5:
            if (0 == strncasecmp(token_text, "false", 5))
                token->type = T_FALSE;            
            else if (0 == strncmp(token_text, "while", 5))
                token->type = T_WHILE;  
            break;          
        case 6:
            if (0 == strncmp(token_text, "return", 6))
                token->type = T_RETURN;
            break;
        case 8:
            if (0 == strncmp(token_text, "function", 8))
                token->type = T_FUNCTION;
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

static char*
read_from_token(Tokenizer *self, Token* token) {
    if (!token->length)
        token->length = self->stream->pos - token->stream_pos;
    // NOTE: read() may return a malloc()d pointer ...
    if (!token->text)
        token->text = self->stream->read(self->stream, token->stream_pos, token->length);
    return token->text;
}

Tokenizer*
tokenizer_init(Stream* stream) {
    Tokenizer * self = calloc(1, sizeof(Tokenizer));
    *self = (Tokenizer) {
        .stream = stream,
        .next = next_token,
        .peek = peek_token,
        .fetch_text = read_from_token,
    };
    return self;
}
