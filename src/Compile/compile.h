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
    int                 index    :8;    // Register holding result
    int                 location :4;
    unsigned int        islookup :1;
} CompileResult;

typedef struct block_compile_result {
    CompileResult       result;
    CodeBlock           *block;
} BlockCompileResult;

void print_codeblock(const CodeContext*, const CodeBlock*);
void print_instructions(const CodeContext*, const void*, int);
CodeContext* compile_string(Compiler *self, const char * text, size_t length);
CodeContext* compile_file(Compiler *self, FILE *restrict input);
CodeContext* compile_ast(Compiler*, ASTNode*);

struct token_to_math_op {
    enum token_type     token;
    enum opcode         vm_opcode;
    enum lox_vm_math    math_op;
    const char          *op_desc;
};

struct token_to_compare_op {
    enum token_type     token;
    enum opcode         vm_opcode;
    enum lox_vm_compare compare_op;
    const char          *op_desc;
};

#ifndef _token_to_math_op
#define _token_to_math_op

static struct token_to_math_op
TokenToMathOp[] = {
    { T_OP_PLUS,        ROP_MATH,    MATH_BINARY_PLUS,       "+" },
    { T_OP_MINUS,       ROP_MATH,    MATH_BINARY_MINUS,      "-" },
    { T_OP_STAR,        ROP_MATH,    MATH_BINARY_STAR,       "*" },
    { T_OP_SLASH,       ROP_MATH,    MATH_BINARY_SLASH,      "/" },
    { T_OP_AMPERSAND,   ROP_MATH,    MATH_BINARY_AND,        "&" },
    { T_OP_PIPE,        ROP_MATH,    MATH_BINARY_OR,         "|" },
    { T_OP_CARET,       ROP_MATH,    MATH_BINARY_XOR,        "^" },
    { T_OP_PERCENT,     ROP_MATH,    MATH_BINARY_MODULUS,    "%" },
    { T_OP_LSHIFT,      ROP_MATH,    MATH_BINARY_LSHIFT,     "<<" },
    { T_OP_RSHIFT,      ROP_MATH,    MATH_BINARY_RSHIFT,     ">>" },

    { T_OP_EQUAL,       ROP_COMPARE,    COMPARE_EQ,             "==" },
    { T_OP_NOTEQUAL,    ROP_COMPARE,    COMPARE_NOT_EQ,         "!=" },
    { T_OP_LT,          ROP_COMPARE,    COMPARE_LT,             "<" },
    { T_OP_LTE,         ROP_COMPARE,    COMPARE_LTE,            "<=" },
    { T_OP_GT,          ROP_COMPARE,    COMPARE_GT,             ">" },
    { T_OP_GTE,         ROP_COMPARE,    COMPARE_GTE,            ">=" },
    { T_OP_IN,          ROP_COMPARE,    COMPARE_IN,             "in" },
};

static struct token_to_compare_op
TokenToCompareOp[] = {
    { T_OP_EQUAL,       ROP_COMPARE,     COMPARE_EQ,     "==" },
    { T_OP_NOTEQUAL,    ROP_COMPARE,     COMPARE_NOT_EQ, "!=" },
    { T_OP_GT,          ROP_COMPARE,     COMPARE_GT,     ">"  },
    { T_OP_GTE,         ROP_COMPARE,     COMPARE_GTE,    ">=" },
    { T_OP_LT,          ROP_COMPARE,     COMPARE_LT,     "<"  },
    { T_OP_LTE,         ROP_COMPARE,     COMPARE_LTE,    "<=" },
    { T_OP_IN,          ROP_COMPARE,     COMPARE_IN,     "in" },
};
#endif

#endif
