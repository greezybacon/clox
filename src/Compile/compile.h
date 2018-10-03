#ifndef COMPILE_COMPILE_H
#define COMPILE_COMPILE_H

#include "vm.h"
#include <stdio.h>

void print_codeblock(CodeContext*, CodeBlock*);
void print_instructions(CodeContext*, Instruction*, int);
CodeContext* compile_string(Compiler *self, const char * text, size_t length);
CodeContext* compile_file(Compiler *self, FILE *restrict input);

#endif