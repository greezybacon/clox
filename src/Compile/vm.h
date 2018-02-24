#ifndef COMPILE_VM_H
#define COMPILE_VM_H

#include "Include/Lox.h"

enum opcode {
    // Basic
    OP_NOOP = 0,
    OP_JUMP,
    OP_POP_JUMP_IF_TRUE,
    OP_POP_JUMP_IF_FALSE,
    OP_DUP_TOP,
    OP_CALL,
    OP_RETURN,

    OP_LOOKUP,
    OP_LOOKUP_LOCAL,
    OP_STORE,
    OP_STORE_LOCAL,
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

typedef struct constant_object {
    Object              *value;
    hashval_t           hash;
} Constant;

typedef struct locals_list {
    Object              **names;
    unsigned            size;
    unsigned            count;
} LocalsList;

typedef struct code_context {
    unsigned            nConstants;
    unsigned            nParameters;
    CodeBlock           *block;
    unsigned            sizeConstants;
    Constant            *constants;
    LocalsList          locals;
    struct code_context *prev;
} CodeContext;

typedef struct compiler_object {
    CodeContext         *context;
} Compiler;

#define JUMP_LENGTH(block) ((block)->nInstructions)

typedef struct eval_context {
    unsigned            stack_size;
    Instruction         *pc;
    Object              **stack;
    Object              **locals;
} EvalContext;

#define POP(context) *(--((context)->stack))
#define PEEK(context) *context->stack
#define PUSH(context, what) *((context)->stack++) = (what)
#define JUMP(context, len) (context)->pc += (len)

#define PRINT(value) do { \
    StringObject *S = (StringObject*) ((Object*) value)->type->as_string((Object*) value); \
    printf("(%s) %.*s #%d\n", ((Object*) value)->type->name, S->length, S->characters, \
        ((Object*) value)->refcount); \
    DECREF(S); \
} while(0)

#endif