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

typedef struct register_status {
    uint32_t            used[8];       // Up to 256 registers
    uint8_t             lowest_avail;
    short               high_water_mark;
} RegisterStatus;

typedef struct compiler_info {
    Object              *function_name;
    Object              *class_name;
    struct compiler_info *prev;
} CompileInfo;

typedef struct compiler_object {
    CodeContext         *context;
    CompileInfo         *info;
    unsigned            flags;
    RegisterStatus      registers;
} Compiler;

typedef struct compile_result {
    unsigned            length;         // TODO: Retire this usage
    unsigned            opcodes;        // Count of opcodes
    unsigned            bytes;          // Length of bytecode
    unsigned            index    :8;    // Register holding result
    enum op_var_location_type location :4;
    unsigned            islookup :1;
} CompileResult;

typedef struct block_compile_result {
    CompileResult       result;
    CodeBlock           *block;
} BlockCompileResult;

void print_codeblock(const CodeContext*, const CodeBlock*);
void print_instructions(const CodeContext*, const Instruction*, int);
CodeContext* compile_string(Compiler *self, const char * text, size_t length);
CodeContext* compile_file(Compiler *self, FILE *restrict input);
CodeContext* compile_ast(Compiler*, ASTNode*);

#endif
