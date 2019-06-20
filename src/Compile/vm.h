#ifndef COMPILE_VM_H
#define COMPILE_VM_H

#include <stdint.h>

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

    // Register-based opcodes
    ROP_STORE,          // flags, dst, src
    ROP_MATH,           // flags, op, lhs, rhs, out, flags=nfllrroo where l, r, and o are op_var_location_type,
                        //      n mean negate (-) and f means logical not (!)
    ROP_COMPARE,        // flags, op, lhs, rhs, out, flags=.fllrroo where f = logical flip (not, !)
    ROP_CALL,           // flags, fun, out, len, params[], flags=vlrtffoo where f and o are op_var_location_type,
                        //      r is recurse, t is tail-call, l is long args instead of short, v is void (return
                        //      value ignored)
    ROP_CALL_RECURSE,   // flags, op, out, len, params[], same as CALL, but op is implied to be current function
    ROP_ITEM,           // flags, op, object, item, [in / out]
    ROP_ATTR,           // flags, op, object, attr, [in / out], flags=..ooaavv where o, a and v are op_var_location_type
    ROP_CONTROL,        // flags, op (return, continue, break, jump?, loop), value, target.16
                        //      flags=..ll....
    ROP_BUILD,          // flags, op, out, len, params[]
}
__attribute__((packed));

#define ROP_STORE__LEN      4
#define ROP_MATH__LEN       6
#define ROP_COMPARE__LEN    6
#define ROP_ITEM__LEN       6
#define ROP_ATTR__LEN       6
#define ROP_CONTROL__LEN    6
#define ROP_CALL__LEN_BASE  5
#define ROP_BUILD__LEN_BASE 5

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

enum lox_vm_control_op {
    OP_CONTROL_RETURN = 1,
    OP_CONTROL_JUMP,
    OP_CONTROL_JUMP_ABSOLUTE,
    OP_CONTROL_JUMP_IF_TRUE,
    OP_CONTROL_JUMP_IF_FALSE,
    OP_CONTROL_JUMP_ABS_IF_TRUE,
    OP_CONTROL_JUMP_ABS_IF_FALSE,
    OP_CONTROL_LOOP_SETUP,
    OP_CONTROL_LOOP_BREAK,
    OP_CONTROL_LOOP_CONTINUE,
}
__attribute__((packed));

enum lox_vm_build_op {
    OP_BUILD_FUNCTION = 1,
}
__attribute__((packed));

enum lox_vm_attribute_op {
    OP_ATTRIBUTE_GET = 1,
    OP_ATTRIBUTE_SET,
    OP_ATTRIBUTE_DEL,
}
__attribute__((packed));

typedef struct vm_op_four_args {
    union {
        struct {
            uint8_t         t4      :2;
            uint8_t         t3      :2;
            uint8_t         t2      :2;
            uint8_t         t1      :2;
        };
        uint8_t         types;
    };
    uint8_t         args4[1];
}
__attribute__((packed))
ArgsFour;

typedef struct vm_op_short_arg {
    uint8_t     location    :2;
    uint8_t     index       :6;
}
__attribute__((packed))
ShortArg;

typedef struct lox_vm_instruction {
    enum opcode     opcode;
    uint8_t         subtype;
    union {
        uint8_t     all;
        struct {
            uint8_t opt1     :1;
            uint8_t opt2     :1;
            uint8_t lhs      :2;
            uint8_t rhs      :2;
            uint8_t out      :2;
        }           lro;
    }               flags;
    union {
        uint8_t     p1;
        uint8_t     arg;        // DELME: Hold over from stack-based vm
    };
    union {
        struct {
            uint8_t p2;
            uint8_t p3;
        };
        int16_t     p23;
        struct {
            uint8_t len;
            ShortArg      args[1];
        };
    };
} Instruction;

typedef struct lox_vm_short_instruction {
    enum opcode     opcode;
    union {
        uint8_t     all;
        struct {
            uint8_t opt1     :1;
            uint8_t opt2     :1;
            uint8_t lhs      :2;
            uint8_t rhs      :2;
            uint8_t out      :2;
        }           lro;
    }               flags;
    union {
        struct {
            uint8_t p1;
            uint8_t p2;
        };
        int16_t     p12;
    };
} ShortInstruction;

enum op_var_location_type {
    OP_VAR_LOCATE_REGISTER  = 0,
    OP_VAR_LOCATE_CLOSURE,
    OP_VAR_LOCATE_CONSTANT,
    OP_VAR_LOCATE_GLOBAL,
}
__attribute__((packed));

enum op_var_special_constant {
    OP_SPECIAL_CONSTANT_THIS = 254,
    OP_SPECIAL_CONSTANT_SUPER,
}
__attribute__((packed));

typedef struct code_instructions {
    struct code_instructions *prev;
    unsigned            size;
    unsigned            nInstructions;  // TODO: Drop when no longer used
    unsigned            bytes;
    void                *instructions;
} CodeBlock;

typedef struct code_constant {
    Object              *value;
    hashval_t           hash;
} Constant;

typedef struct local_var {
    Constant            name;   // Compile-time names of local vars
    uint8_t             regnum; // Associated register number
} LocalVariable;

typedef struct locals_list {
    LocalVariable       *vars;
    unsigned            size;
    unsigned            count;
} LocalsList;

typedef struct code_constant_list {
    Constant            *items;
    unsigned            size;
    unsigned            count;
} ConstantList;

// Compile-time code context. Represents a compiled block of code / function body.
typedef struct code_context {
    unsigned            nConstants;
    unsigned            nParameters;
    CodeBlock           *block;
    unsigned            sizeConstants;
    Constant            *constants;
    LocalsList          locals;
    short               regs_required;
    short               result_reg;
    struct code_context *prev;
    Object              *owner;             // If defined in a class
} CodeContext;

#define JUMP_LENGTH(block) ((block)->nInstructions)

// Run-time data to evaluate a CodeContext block
typedef struct vmeval_scope {
    struct vmeval_scope *outer;
    LoxTuple            *locals;
    CodeContext         *code;
    LoxTable            *globals;
} VmScope;

VmScope* VmScope_create(VmScope*, CodeContext*, LoxTuple*);
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

#define SetBit(A,k)     ( A[(k/32)] |= (1 << (k%32)) )
#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) )
#define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) )

typedef struct vmeval_context {
    CodeContext     *code;
    VmScope         *scope;
    VmCallArgs      args;
    uint32_t        *regs_used;
    Object          **regs;
    Object          *this;
} VmEvalContext;

Object *vmeval_eval(VmEvalContext*);
Object* vmeval_string(const char*, size_t);
Object* vmeval_string_inscope(const char*, size_t, VmScope*);
Object* vmeval_file(FILE *input);
Object* LoxEval_EvalAST(ASTNode*);
#endif
