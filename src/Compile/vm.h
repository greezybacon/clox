#ifndef COMPILE_VM_H
#define COMPILE_VM_H

#include "Include/Lox.h"

enum opcode {
    // Basic
    OP_NOOP = 0,
    OP_JUMP,
    OP_JUMP_IF_TRUE,
    OP_POP_JUMP_IF_TRUE,
    OP_JUMP_IF_FALSE,
    OP_POP_JUMP_IF_FALSE,
    OP_DUP_TOP,
    OP_POP_TOP,

    OP_CLOSE_FUN,
    OP_CALL_FUN,
    OP_RETURN,

    OP_LOOKUP,
    OP_LOOKUP_LOCAL,
    OP_STORE,
    OP_STORE_LOCAL,
    OP_STORE_ARG_LOCAL,
    OP_CONSTANT,

    // Comparison
    OP_GT,
    OP_GTE,
    OP_LT,
    OP_LTE,
    OP_EQUAL,
    OP_NEQ,

    // Boolean
    OP_BINARY_AND,
    OP_BINARY_OR,
    OP_BANG,

    // Expressions
    OP_BINARY_PLUS,
    OP_BINARY_MINUS,
    OP_BINARY_STAR,
    OP_BINARY_SLASH,
}
__attribute__((packed));

typedef struct instruction {
    enum opcode     op;
    short           arg;
} Instruction;

typedef struct code_instructions {
    struct code_instructions *prev;
    unsigned            size;
    unsigned            nInstructions;
    struct instruction  *instructions;
} CodeBlock;

typedef struct code_constant {
    Object              *value;
    hashval_t           hash;
} Constant;

typedef struct locals_list {
    Constant            *names;     // Compile-time names of local vars
    unsigned            size;
    unsigned            count;
} LocalsList;

// Compile-time code context. Represents a compiled block of code / function body.
typedef struct code_context {
    unsigned            nConstants;
    unsigned            nParameters;
    CodeBlock           *block;
    unsigned            sizeConstants;
    Constant            *constants;
    LocalsList          locals;
    struct code_context *prev;
} CodeContext;

enum compiler_flags {
    CFLAG_LOCAL_VARS =  0x00000001,         // Use local vars where possible
};

typedef struct compiler_object {
    CodeContext         *context;
    unsigned            flags;
} Compiler;

#define JUMP_LENGTH(block) ((block)->nInstructions)

// Run-time data to evaluate a CodeContext block
typedef struct vmeval_scope {
    struct vmeval_scope *outer;
    Object              **locals;
    CodeContext         *code;
    HashObject          *globals;
} VmScope;

VmScope* VmScope_create(VmScope*, Object**, CodeContext*);
void VmScope_assign(VmScope*, Object*, Object*, hashval_t);
Object* VmScope_lookup(VmScope*, Object*, hashval_t);

// Run-time args passing between calls to vmeval_eval
typedef struct vmeval_call_args {
    Object              **values;
    size_t              count;
} VmCallArgs;

#define POP(stack) *(--(stack))
#define PEEK(stack) *stack
#define PUSH(stack, what) *(stack++) = (what)

#define PRINT(value) do { \
    StringObject *S = (StringObject*) ((Object*) value)->type->as_string((Object*) value); \
    printf("(%s) %.*s #%d\n", ((Object*) value)->type->name, S->length, S->characters, \
        ((Object*) value)->refcount); \
    DECREF(S); \
} while(0)

typedef struct vmeval_context {
    CodeContext     *code;
    VmScope         *scope;
    VmCallArgs      args;
} VmEvalContext;

Object *vmeval_eval(VmEvalContext*);
Object* vmeval_string(const char*, size_t);
Object* vmeval_string_inscope(const char*, size_t, VmScope*);
Object* vmeval_file(FILE *input);

#endif