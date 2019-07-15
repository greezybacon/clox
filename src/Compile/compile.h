#ifndef COMPILE_COMPILE_H
#define COMPILE_COMPILE_H

#include "vm.h"
#include <stdio.h>

enum compiler_flags {
    CFLAG_LOCAL_VARS =  0x00000001,         // Use local vars where possible
};

enum compiler_special {
    CINFO_CALL_RECURSE = -1,                // Code can recurse rather than call()
};

typedef struct compiler_info {
    Object              *function_name;
    Object              *class_name;
    struct compiler_info *prev;
} CompileInfo;

typedef struct compiler_object {
    CodeContext         *context;
    CompileInfo         *info;
    unsigned            flags;
} Compiler;

void print_codeblock(const CodeContext*, const CodeBlock*);
void print_instructions(const CodeContext*, const Instruction*, int);
CodeContext* compile_string(Compiler *self, const char * text, size_t length);
CodeContext* compile_file(Compiler *self, FILE *restrict input, const char*);
CodeContext* compile_ast(Compiler*, ASTNode*);

#endif
