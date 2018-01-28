#ifndef DEBUG_TOKEN_H
#define DEBUG_TOKEN_H

#include "token.h"

char* get_token_type(enum token_type);
char* get_operator(enum token_type);
void print_token(FILE*, Token*);
void print_token2(FILE*, Token*);

#endif