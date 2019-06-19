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
    { OP_LOOKUP,        "LOOKUP" },
    { OP_LOOKUP_LOCAL,  "LOOKUP_LOCAL" },
    { OP_LOOKUP_GLOBAL, "LOOKUP_GLOBAL" },
    { OP_LOOKUP_CLOSED, "LOOKUP_CLOSED" },
    { OP_STORE,         "STORE" },
    { OP_STORE_LOCAL,   "STORE_LOCAL" },
    { OP_STORE_GLOBAL,  "STORE_GLOBAL" },
    { OP_STORE_CLOSED,  "STORE_CLOSED" },
    { OP_STORE_ARG_LOCAL, "STORE_ARG_LOCAL" },
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

    // Register-based opcodes
    { ROP_STORE,        "R-STORE" },
    { ROP_MATH,         "R-MATH" },
    { ROP_COMPARE,      "R-COMPARE" },
    { ROP_CONTROL,      "R-CONTROL" },
    { ROP_CALL,         "R-INVOKE" },
    { ROP_BUILD,        "R-BUILD" },
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

static void
print_arg_indirect(const CodeContext *context, unsigned index,
    enum op_var_location_type location
) {
    switch (location) {
    case OP_VAR_LOCATE_REGISTER:
        // TODO: See if this register is a local
        printf("r#%d", index);
        break;
    case OP_VAR_LOCATE_CONSTANT: {
        Constant *C = context->constants + index;
        Object *T = C->value;
        LoxString *S = String_fromObject(T);
        printf("<literal> %.*s", S->length, S->characters);
        break;
    }
    case OP_VAR_LOCATE_CLOSURE: {
        CodeContext *outer = context->prev;
        if (outer) {
            assert(outer->locals.count > index);
            Object *T = (outer->locals.vars + index)->name.value;
            LoxString *S = String_fromObject(T);
            printf("<nonlocal> %.*s", S->length, S->characters);
        }
        break;
    }
    case OP_VAR_LOCATE_GLOBAL: {
        Constant *C = context->constants + index;
        Object *T = C->value;
        LoxString *S = String_fromObject(T);
        printf("<global> %.*s", S->length, S->characters);
        break;
    }}
}

static inline unsigned
print_opcode(const CodeContext *context, const Instruction *op) {
    struct named_opcode* T, key = { .code = op->opcode }, unknown;
    T = bsearch(&key, OpcodeNames, sizeof(OpcodeNames) / sizeof(struct named_opcode),
        sizeof(struct named_opcode), cmpfunc);

    if (T == NULL) {
        unknown = (struct named_opcode) {
            .name = "(unknown)",
        };
        T = &unknown;
    }

    unsigned length = 3;
    printf("%-20s", T->name);

    // For opcodes which use constants, print the constant value too
    switch (op->opcode) {
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
    case OP_LOOKUP_LOCAL: {
        Object *T = (context->locals.vars + op->arg)->name.value;
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
            Object *T = (outer->locals.vars + op->arg)->name.value;
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

    case ROP_STORE: {
        ShortInstruction *sop = (ShortInstruction*) op;
        print_arg_indirect(context, sop->p1, sop->flags.lro.lhs);
        printf(" := ");
        print_arg_indirect(context, sop->p2, sop->flags.lro.rhs);
        length = ROP_STORE__LEN;
        break;
    }

    case ROP_MATH: {
        print_arg_indirect(context, op->p3, op->flags.lro.out);
        printf(" := ");
        print_arg_indirect(context, op->p1, op->flags.lro.lhs);

        struct token_to_math_op* T, key = { .math_op = op->subtype };
        T = bsearch(&key, TokenToMathOp, sizeof(TokenToMathOp) / sizeof(struct token_to_math_op),
            sizeof(struct token_to_math_op), math_cmpfunc);
        printf(" %s ", T->op_desc);
        print_arg_indirect(context, op->p2, op->flags.lro.rhs);
        length = ROP_MATH__LEN;
        break;
    }

    case ROP_COMPARE: {
        print_arg_indirect(context, op->p3, op->flags.lro.out);
        printf(" := ");
        print_arg_indirect(context, op->p1, op->flags.lro.lhs);

        struct token_to_compare_op* T, key = { .compare_op = op->subtype };
        T = bsearch(&key, TokenToCompareOp, sizeof(TokenToCompareOp) / sizeof(struct token_to_compare_op),
            sizeof(struct token_to_compare_op), math_cmpfunc);
        printf(" %s ", T->op_desc);
        print_arg_indirect(context, op->p2, op->flags.lro.rhs);
        length = ROP_COMPARE__LEN;
        break;
    }

    case ROP_CONTROL: {
        switch (op->subtype) {
        case OP_CONTROL_JUMP_IF_TRUE:
        case OP_CONTROL_JUMP_IF_FALSE:
            printf("if %s(", op->subtype == OP_CONTROL_JUMP_IF_FALSE ? "!" : "");
            print_arg_indirect(context, op->p1, op->flags.lro.lhs);
            printf(") ");
        case OP_CONTROL_JUMP:
            printf("jump %d", op->p23);
            break;
        case OP_CONTROL_RETURN:
            printf("return ");
            print_arg_indirect(context, op->p1, op->flags.lro.lhs);
            break;
        case OP_CONTROL_LOOP_SETUP:
        case OP_CONTROL_LOOP_BREAK:
        case OP_CONTROL_LOOP_CONTINUE:
            break;
        }
        length = ROP_CONTROL__LEN;
        break;
    }

    case ROP_CALL_RECURSE:
    case ROP_CALL: {
        unsigned count = op->len;
        const ShortArg *args = op->args;

        if (!op->flags.lro.opt1) {
            print_arg_indirect(context, op->p1, op->flags.lro.out);
            printf(" := ");
        }
        if (op->opcode == ROP_CALL)
            print_arg_indirect(context, op->subtype, op->flags.lro.rhs);
        else
            printf("recurse");
        printf("(");
        while (count--) {
            // TODO: Handle long arguments
            print_arg_indirect(context, args->index, args->location);
            if (count)
                printf(", ");
            args++;
        }
        printf(")");
        length = ROP_CALL__LEN_BASE + op->len;
        break;
    }

    // ROP_BUILD + flags, op, out, len, params[]
    case ROP_BUILD: {
        switch ((enum lox_vm_build_op) op->subtype) {
        case OP_BUILD_FUNCTION:
            print_arg_indirect(context, op->p1, op->flags.lro.out);
            printf(" := ");
            print_arg_indirect(context, op->p2, op->flags.lro.rhs);
            length = ROP_BUILD__LEN_BASE;
            break;
        }

        break;
    }

    default:
        break;
    }
    printf("\n");
    return length;
}

void
print_instructions(const CodeContext *context, const void *block, int bytes) {
    static bool sorted = false;
    if (!sorted) {
        qsort(OpcodeNames, sizeof(OpcodeNames) / sizeof(struct named_opcode),
            sizeof(struct named_opcode), cmpfunc);
        qsort(TokenToMathOp, sizeof(TokenToMathOp) / sizeof(struct token_to_math_op),
            sizeof(struct token_to_math_op), math_cmpfunc);
        qsort(TokenToCompareOp, sizeof(TokenToCompareOp) / sizeof(struct token_to_compare_op),
            sizeof(struct token_to_compare_op), math_cmpfunc);
    }

    unsigned length, start=0;
    while (bytes > 0) {
        printf("%-5d ", start);
        length = print_opcode(context, block);
        block += length;
        start += length;
        bytes -= length;
    }
}

void
print_codeblock(const CodeContext* context, const CodeBlock *block) {
    print_instructions(context, block->instructions, block->bytes);
}