#ifndef COMPILE_VM_H
#define COMPILE_VM_H

#include "Include/Lox.h"
#include "Objects/class.h"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

enum opcode {
    // Basic
    OP_NOOP = 0,
    OP_HALT,
    OP_JUMP,
    OP_JUMP_IF_TRUE,
    OP_POP_JUMP_IF_TRUE,
    OP_JUMP_IF_FALSE,
    OP_POP_JUMP_IF_FALSE,
    OP_JUMP_IF_FALSE_OR_POP,
    OP_JUMP_IF_TRUE_OR_POP,
    OP_DUP_TOP,
    OP_POP_TOP,

    // Functions
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
    OP_CONSTANT,

    // Comparison
    OP_COMPARE,

    // Boolean
    OP_BANG,

    // Expressions
    OP_BINARY_MATH,
    OP_UNARY_NEGATIVE,
    OP_UNARY_INVERT,

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

    // Loops
    OP_ENTER_BLOCK,
    OP_LEAVE_BLOCK,
    OP_BREAK,
    OP_CONTINUE,
    OP_GET_ITERATOR,
    OP_NEXT_OR_BREAK,
}
__attribute__((packed));

enum lox_vm_math {
    MATH_BINARY_PLUS = 0,
    MATH_BINARY_MINUS,
    MATH_BINARY_STAR,
    MATH_BINARY_POW,
    MATH_BINARY_SLASH,
    MATH_BINARY_MODULUS,
    MATH_BINARY_LSHIFT,
    MATH_BINARY_RSHIFT,
    MATH_BINARY_AND,
    MATH_BINARY_OR,
    MATH_BINARY_XOR,
    __MATH_BINARY_MAX,
}
__attribute__((packed));

typedef Object* (*lox_vm_binary_math_func)(Object*, Object*);

enum lox_vm_compare {
    COMPARE_IS = 1,
    COMPARE_EQ,
    COMPARE_NOT_EQ,
    COMPARE_EXACT,
    COMPARE_NOT_EXACT,
    COMPARE_LT,
    COMPARE_LTE,
    COMPARE_GT,
    COMPARE_GTE,
    COMPARE_IN,
    COMPARE_SPACESHIP,
}
__attribute__((packed));

typedef struct code_source {
    unsigned    opcode_count;           // Offset in instruction list to this line
    unsigned    line_number;            // Line number of code
} CodeSource;

typedef struct code_source_list {
    const char  *filename;              // File where the code came from
    unsigned    size, count;            // Size of the CodeSource list and current usage
    CodeSource  *offsets;
} CodeSourceList;

typedef struct instruction {
    enum opcode     op;
    short           arg;
} Instruction;

typedef struct instruction_list {
    unsigned        size, count;        // Size of the Instruction list and current usage
    Instruction     *opcodes;
} InstructionList;

typedef struct code_instructions {
    struct code_instructions *prev;
    InstructionList     instructions;
    CodeSourceList      codesource;
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

#define JUMP_LENGTH(block) ((block)->instructions.count)

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
    if (NULL == value) { \
        printf("(null)\n"); \
    } \
    else { \
        LoxString *S = String_fromObject(value); \
        INCREF(S); \
        printf("(%s) %.*s\n", ((Object*) value)->type->name, S->length, S->characters); \
        DECREF(S); \
    } \
} while(0)

typedef struct vmeval_context {
    CodeContext     *code;
    VmScope         *scope;
    VmCallArgs      args;
    Object          *this;
    Instruction     **pc;
    struct vmeval_context *previous;
} VmEvalContext;

typedef struct vmeval_loop_block {
    Instruction     *top;
    Instruction     *bottom;
} VmEvalLoopBlock;

Object *vmeval_eval(VmEvalContext*);
Object* vmeval_string(const char*, size_t);
Object* vmeval_string_inscope(const char*, size_t, VmScope*);
Object* vmeval_file(FILE *input, const char*);
Object* vmeval_file_inscope(FILE *input, const char*, VmScope*);
Object* LoxEval_EvalAST(ASTNode*);
#endif
