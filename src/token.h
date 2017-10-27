#ifndef TOKEN_H
#define TOKEN_H

#include "stream.h"

enum token_type {
    T_EOF = 0,
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
    T_OP_LT,
    T_OP_LTE,
    T_OP_GT,
    T_OP_GTE,
    T_OP_EQUAL,
    T_OP_ASSIGN,
    T_BANG,
    T_AND,
    T_OR,
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
    T_DOT,
    T_SEMICOLON,
    T_OPEN_BRACE,
    T_CLOSE_BRACE,
    T_COMMA,
};

typedef struct token {
    enum token_type     type;
    int                 line;
    int                 pos;
    char*               text;
    int                 length;
} Token;

typedef struct tokenize_context {
    Stream*     stream;

    Token*      (*next)(struct tokenize_context*);
    Token*      (*peek)(struct tokenize_context*);
    Token*      current;
    Token*      _peek;
} Tokenizer;

Tokenizer*
tokenizer_init(Stream*);

#endif
