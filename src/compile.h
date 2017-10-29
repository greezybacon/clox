#include "parse.h"

#ifndef COMPILE_H
#define COMPILE_H

typedef struct compiler_context {
    Parser *    parser;
    void        (*compile)(struct compiler_context*, struct parser_context*);
} Compiler;

int compiler_init(Compiler*);

#endif