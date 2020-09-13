#ifndef TOKEN_H
#define TOKEN_H

#include "stream.h"

enum token_type {
    T_EOF = -1,
    T_WHITESPACE = 1,
    T_TRUE,
    T_FALSE,
    T_NUMBER,
    T_STRING,
    T_WORD,
    T_NULL,
    T__OP_MIN,
    T_OP_PLUS,
    T_OP_MINUS,
    T_OP_STAR,
    T_OP_SLASH,
    T_OP_CARET,
    T_OP_PERCENT,
    T_OP_LSHIFT,
    T_OP_RSHIFT,
    T_OP_AMPERSAND,
    T_OP_PIPE,
    T_OP_TILDE,
    T_OP_LT,
    T_OP_LTE,
    T_OP_GT,
    T_OP_GTE,
    T_OP_EQUAL,
    T_OP_NOTEQUAL,
    T_OP_ASSIGN,
    T_OP_IN,
    T_OP_IS,
    T_BANG,
    T_AND,
    T_OR,
    T_DOT,
    T__OP_MAX,
    T_OPEN_PAREN,
    T_CLOSE_PAREN,
    T_VAR,
    T_IF,
    T_ELSE,
    T_WHILE,
    T_FOR,
    T_FUNCTION,
    T_CLASS,
    T_SEMICOLON,
    T_OPEN_BRACE,
    T_CLOSE_BRACE,
    T_OPEN_BRACKET,
    T_CLOSE_BRACKET,
    T_COMMA,
    T_RETURN,
    T_COMMENT,
    T_THIS,
    T_SUPER,
    T_COLON,
    T_FOREACH,
    T_CONTINUE,
    T_BREAK,
    T_ASSERT,
};

typedef struct token {
    enum token_type     type;
    int                 line;
    int                 pos;
    int                 stream_pos;
    int                 length;
    const char*         text;
} Token;

typedef struct tokenize_context {
    Stream*     stream;

    Token*      (*next)(struct tokenize_context*);
    Token*      (*peek)(struct tokenize_context*);
    const char* (*fetch_text)(struct tokenize_context*, struct token*);
    Token*      current;
    Token*      previous;
    Token*      _peek;
} Tokenizer;

Tokenizer*
tokenizer_init(Stream*);

#endif
