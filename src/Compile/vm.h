#ifndef COMPILE_VM_H
#define COMPILE_VM_H

#include "Include/Lox.h"
#include "Objects/class.h"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

enum opcode {
    // Basic
    OP_NOOP = 0,
    OP_JUMP,
    OP_JUMP_IF_TRUE,
    OP_POP_JUMP_IF_TRUE,
    OP_JUMP_IF_FALSE,
    OP_POP_JUMP_IF_FALSE,
    OP_JUMP_IF_FALSE_OR_POP,
    OP_JUMP_IF_TRUE_OR_POP,
    OP_DUP_TOP,
    OP_POP_TOP,

    OP_CLOSE_FUN,
    OP_CALL_FUN,
    OP_RETURN,
    OP_RECURSE,

    // Scope
    OP_LOOKUP,
    OP_LOOKUP_LOCAL,
    OP_LOOKUP_GLOBAL,
    OP_LOOKUP_CLOSED,
    OP_STORE,
    OP_STORE_LOCAL,
    OP_STORE_GLOBAL,
    OP_STORE_CLOSED,
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
    OP_BANG,
    OP_IN,

    // Expressions
    OP_MATH,

    // Classes
    OP_BUILD_CLASS,
    OP_BUILD_SUBCLASS,
    OP_GET_ATTR,
    OP_SET_ATTR,
    OP_THIS,
    OP_SUPER,

    // Hash and list types
    OP_GET_ITEM,
    OP_SET_ITEM,
    OP_DEL_ITEM,

    // Complex literals
    OP_BUILD_TUPLE,
    OP_BUILD_STRING,
    OP_FORMAT,
    OP_BUILD_TABLE,
}
__attribute__((packed));

enum lox_vm_math {
    MATH_BINARY_AND = 1,
    MATH_BINARY_OR,
    MATH_BINARY_XOR,
    MATH_BINARY_PLUS,
    MATH_BINARY_MINUS,
    MATH_BINARY_STAR,
    MATH_BINARY_SLASH,
    MATH_BINARY_LSHIFT,
    MATH_BINARY_RSHIFT,
    MATH_BINARY_MODULUS,
    MATH_UNARY_NEGATIVE,
    MATH_UNARY_INVERT,
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
    Object              *owner;             // If defined in a class
} CodeContext;

#define JUMP_LENGTH(block) ((block)->nInstructions)

// Run-time data to evaluate a CodeContext block
typedef struct vmeval_scope {
    struct vmeval_scope *outer;
    Object              **locals;
    size_t              locals_count;
    CodeContext         *code;
    LoxTable            *globals;
} VmScope;

VmScope* VmScope_create(VmScope*, CodeContext*, Object**, unsigned);
void VmScope_assign(VmScope*, Object*, Object*, hashval_t);
Object* VmScope_lookup_global(VmScope*, Object*, hashval_t);
Object* VmScope_lookup_local(VmScope*, unsigned);
VmScope* VmScope_leave(VmScope*);

// Run-time args passing between calls to vmeval_eval
typedef struct vmeval_call_args {
    Object              **values;
    size_t              count;
} VmCallArgs;

#define POP(stack) *(--(stack))
#define XPOP(stack) do { --(stack); DECREF(*stack); } while(0)
#define PEEK(stack) *(stack - 1)
#define PUSH(stack, what) ({ \
    typeof(what) _what = (what); \
    *(stack++) = _what; \
    INCREF((Object*) _what); \
})
#define XPUSH(stack, what) *(stack++) = (what)

#define PRINT(value) do { \
    LoxString *S = (LoxString*) ((Object*) value)->type->as_string((Object*) value); \
    printf("(%s) %.*s #%d\n", ((Object*) value)->type->name, S->length, S->characters); \
} while(0)

typedef struct vmeval_context {
    CodeContext     *code;
    VmScope         *scope;
    VmCallArgs      args;
    Object          *this;
} VmEvalContext;

Object *vmeval_eval(VmEvalContext*);
Object* vmeval_string(const char*, size_t);
Object* vmeval_string_inscope(const char*, size_t, VmScope*);
Object* vmeval_file(FILE *input);
Object* LoxEval_EvalAST(ASTNode*);
#endif