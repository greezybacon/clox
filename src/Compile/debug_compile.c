#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "vm.h"

struct named_opcode {
    enum opcode     code;
    char*           name;
};

static struct named_opcode OpcodeNames[] = {
    // Basic
    { OP_NOOP,          "NOOP" },
    { OP_HALT,          "HALT" },
    { OP_JUMP,          "JUMP" },
    { OP_POP_JUMP_IF_TRUE, "POP_JUMP_IF_TRUE" },
    { OP_POP_JUMP_IF_FALSE, "POP_JUMP_IF_FALSE" },
    { OP_JUMP_IF_TRUE, "JUMP_IF_TRUE" },
    { OP_JUMP_IF_FALSE, "JUMP_IF_FALSE" },
    { OP_JUMP_IF_TRUE_OR_POP, "JUMP_IF_TRUE_OR_POP" },
    { OP_JUMP_IF_FALSE_OR_POP, "JUMP_IF_FALSE_OR_POP" },
    { OP_DUP_TOP,       "DUP_TOP" },
    { OP_POP_TOP,       "POP_TOP" },

    // Functions
    { OP_CALL_FUN,      "CALL_FUNCTION" },
    { OP_CLOSE_FUN,     "CLOSE_FUNCTION" },
    { OP_RETURN,        "RETURN" },
    { OP_RECURSE,       "RECURSE_CALL" },

    // Variables
    { OP_LOOKUP_LOCAL,  "LOOKUP_LOCAL" },
    { OP_LOOKUP_GLOBAL, "LOOKUP_GLOBAL" },
    { OP_LOOKUP_CLOSED, "LOOKUP_CLOSED" },
    { OP_STORE_LOCAL,   "STORE_LOCAL" },
    { OP_STORE_GLOBAL,  "STORE_GLOBAL" },
    { OP_STORE_CLOSED,  "STORE_CLOSED" },
    { OP_CONSTANT,      "CONSTANT" },

    // Comparison
    { OP_COMPARE,       "COMPARE" },

    // Boolean logic
    { OP_BANG,          "BANG (!)" },

    // Expressions / math
    { OP_BINARY_MATH,   "BINARY MATH" },
    { OP_UNARY_NEGATIVE,"NEGATIVE" },
    { OP_UNARY_INVERT,  "INVERT" },

    // Classes
    { OP_BUILD_CLASS,   "BUILD_CLASS" },
    { OP_BUILD_SUBCLASS, "BUILD_SUBCLASS" },
    { OP_GET_ATTR,      "GET_ATTRIBUTE" },
    { OP_SET_ATTR,      "SET_ATTRIBUTE" },
    { OP_THIS,          "THIS" },
    { OP_SUPER,         "SUPER" },

    // Hash and list types
    { OP_GET_ITEM,      "GET_ITEM" },
    { OP_SET_ITEM,      "SET_ITEM" },
    { OP_DEL_ITEM,      "DEL_ITEM" },

    // Complex literals
    { OP_BUILD_TUPLE,   "BUILD_TUPLE" },
    { OP_BUILD_STRING,  "BUILD_STRING" },
    { OP_FORMAT,        "APPLY_FORMAT" },
    { OP_BUILD_TABLE,   "BUILD_TABLE" },

    // Loops
    { OP_ENTER_BLOCK,   "ENTER_BLOCK" },
    { OP_LEAVE_BLOCK,   "LEAVE_BLOCK" },
    { OP_BREAK,         "BREAK" },
    { OP_CONTINUE,      "CONTINUE" },
    { OP_GET_ITERATOR,  "GET_ITERATOR" },
    { OP_NEXT_OR_BREAK, "NEXT_OR_BREAK" },
};

static int cmpfunc (const void * a, const void * b) {
   return ((struct named_opcode*) a)->code - ((struct named_opcode*) b)->code;
}

static struct token_to_math_op {
    enum token_type     token;
    enum opcode         vm_opcode;
    enum lox_vm_math    math_op;
    const char          *op_desc;
} TokenToMathOp[] = {
    { T_OP_PLUS,        OP_BINARY_MATH,    MATH_BINARY_PLUS,       "+" },
    { T_OP_MINUS,       OP_BINARY_MATH,    MATH_BINARY_MINUS,      "-" },
    { T_OP_STAR,        OP_BINARY_MATH,    MATH_BINARY_STAR,       "*" },
    { T_OP_SLASH,       OP_BINARY_MATH,    MATH_BINARY_SLASH,      "/" },
    { T_OP_AMPERSAND,   OP_BINARY_MATH,    MATH_BINARY_AND,        "&" },
    { T_OP_PIPE,        OP_BINARY_MATH,    MATH_BINARY_OR,         "|" },
    { T_OP_CARET,       OP_BINARY_MATH,    MATH_BINARY_XOR,        "^" },
    { T_OP_PERCENT,     OP_BINARY_MATH,    MATH_BINARY_MODULUS,    "%" },
    { T_OP_LSHIFT,      OP_BINARY_MATH,    MATH_BINARY_LSHIFT,     "<<" },
    { T_OP_RSHIFT,      OP_BINARY_MATH,    MATH_BINARY_RSHIFT,     ">>" },
};

static struct token_to_compare_op {
    enum token_type     token;
    enum opcode         vm_opcode;
    enum lox_vm_compare compare_op;
    const char          *op_desc;
} TokenToCompareOp[] = {
    { T_OP_EQUAL,       OP_COMPARE,     COMPARE_EQ,     "==" },
    { T_OP_NOTEQUAL,    OP_COMPARE,     COMPARE_NOT_EQ, "!=" },
    { T_OP_GT,          OP_COMPARE,     COMPARE_GT,     ">"  },
    { T_OP_GTE,         OP_COMPARE,     COMPARE_GTE,    ">=" },
    { T_OP_LT,          OP_COMPARE,     COMPARE_LT,     "<"  },
    { T_OP_LTE,         OP_COMPARE,     COMPARE_LTE,    "<=" },
    { T_OP_IN,          OP_COMPARE,     COMPARE_IN,     "in" },
};

static int math_cmpfunc (const void * a, const void * b) {
   return ((struct token_to_math_op*) a)->math_op - ((struct token_to_math_op*) b)->math_op;
}

static inline void
print_opcode(const CodeContext *context, const Instruction *op) {
    struct named_opcode* T, key = { .code = op->op }, unknown;
    T = bsearch(&key, OpcodeNames, sizeof(OpcodeNames) / sizeof(struct named_opcode),
        sizeof(struct named_opcode), cmpfunc);

    if (T == NULL) {
        unknown = (struct named_opcode) {
            .name = "(unknown)",
        };
        T = &unknown;
    }

    printf("%-20s %d", T->name, op->arg);

    // For opcodes which use constants, print the constant value too
    switch (op->op) {
    case OP_FORMAT:
    case OP_GET_ATTR:
    case OP_SET_ATTR:
    case OP_CONSTANT:
    case OP_STORE:
    case OP_LOOKUP:
    case OP_LOOKUP_GLOBAL:
    case OP_STORE_GLOBAL: {
        Constant *C = context->constants + op->arg;
        Object *T = C->value;
        if (T && T->type && T->type->as_string) {
            LoxString *S = (LoxString*) T->type->as_string(T);
            assert(String_isString((Object*) S));
            printf(" (%.*s)", S->length, S->characters);
        }
    }
    break;

    case OP_STORE_LOCAL:
    case OP_LOOKUP_LOCAL:
    case OP_NEXT_OR_BREAK: {
        Object *T = (context->locals.names + op->arg)->value;
        if (T && T->type && T->type->as_string) {
            LoxString *S = (LoxString*) T->type->as_string(T);
            assert(String_isString((Object*) S));
            printf(" (%.*s)", S->length, S->characters);
        }
    }
    break;

    case OP_STORE_CLOSED:
    case OP_LOOKUP_CLOSED: {
        CodeContext *outer = context->prev;
        if (outer) {
            Object *T = (outer->locals.names + op->arg)->value;
            if (T && T->type && T->type->as_string) {
                LoxString *S = (LoxString*) T->type->as_string(T);
                assert(String_isString((Object*) S));
                printf(" (%.*s)", S->length, S->characters);
            }
        }
    }
    break;

    case OP_BINARY_MATH: {
        struct token_to_math_op* T, key = { .math_op = op->arg };
        T = bsearch(&key, TokenToMathOp, sizeof(TokenToMathOp) / sizeof(struct token_to_math_op),
            sizeof(struct token_to_math_op), math_cmpfunc);
            printf(" (%s)", T->op_desc);
    }
    break;

    case OP_COMPARE: {
        struct token_to_compare_op* T, key = { .compare_op = op->arg };
        T = bsearch(&key, TokenToCompareOp, sizeof(TokenToCompareOp) / sizeof(struct token_to_compare_op),
            sizeof(struct token_to_compare_op), math_cmpfunc);
            printf(" (%s)", T->op_desc);
    }
    break;

    default:
        break;
    }
    printf("\n");
}

void
print_instructions(const CodeContext *context, const Instruction *block, int count, int start) {
    static bool sorted = false;
    if (!sorted) {
        qsort(OpcodeNames, sizeof(OpcodeNames) / sizeof(struct named_opcode),
            sizeof(struct named_opcode), cmpfunc);
        qsort(TokenToMathOp, sizeof(TokenToMathOp) / sizeof(struct token_to_math_op),
            sizeof(struct token_to_math_op), math_cmpfunc);
        qsort(TokenToCompareOp, sizeof(TokenToCompareOp) / sizeof(struct token_to_compare_op),
            sizeof(struct token_to_compare_op), math_cmpfunc);
    }

    int i = 0;
    while (count--) {
        printf("% 8d  ", start + i++);
        print_opcode(context, block++);
    }
}

void
print_codeblock(const CodeContext* context, const CodeBlock *block) {
    int total = block->instructions.count,
        start = 0,
        lines = block->codesource.count,
        prev_line = 0;
    CodeSource *source = block->codesource.offsets;

    while (lines--) {
        // Emit all the instructions from the same line number
            printf("Line %d\n", source->line_number);

        print_instructions(context, block->instructions.opcodes + start, source->opcode_count, start);
        start += source->opcode_count;
        source++;
    }
}
