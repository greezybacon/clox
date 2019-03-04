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
    { OP_GT,            "GT (>)" },
    { OP_GTE,           "GTE (>=)" },
    { OP_LT,            "LT (<)" },
    { OP_LTE,           "LTE (<=)" },
    { OP_EQUAL,         "EQUAL (==)" },
    { OP_NEQ,           "NEQ (!=)" },

    // Boolean logic
    { OP_BINARY_AND,    "AND (&&)" },
    { OP_BINARY_OR,     "OR (||)" },
    { OP_BANG,          "BANG (!)" },
    { OP_IN,            "IN" },

    // Expressions / math
    { OP_BINARY_PLUS,   "PLUS (+)" },
    { OP_BINARY_MINUS,  "MINUS (-)" },
    { OP_BINARY_STAR,   "STAR (*)" },
    { OP_BINARY_SLASH,  "SLASH (/)" },
    { OP_NEG,           "NEG (-)" },

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
};

static int cmpfunc (const void * a, const void * b) {
   return ((struct named_opcode*) a)->code - ((struct named_opcode*) b)->code;
}

static inline void
print_opcode(const CodeContext *context, const Instruction *op) {
    struct named_opcode* T, key = { .code = op->op };
    T = bsearch(&key, OpcodeNames, sizeof(OpcodeNames) / sizeof(struct named_opcode),
        sizeof(struct named_opcode), cmpfunc);

    printf("%-20s %d", T->name, op->arg);

    // For opcodes which use constants, print the constant value too
    switch (op->op) {
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
            StringObject *S = (StringObject*) T->type->as_string(T);
            assert(String_isString((Object*) S));
            printf(" (%.*s)", S->length, S->characters);
        }
    }
    break;

    case OP_STORE_LOCAL:
    case OP_LOOKUP_LOCAL: {
        Object *T = (context->locals.names + op->arg)->value;
        if (T && T->type && T->type->as_string) {
            StringObject *S = (StringObject*) T->type->as_string(T);
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
                StringObject *S = (StringObject*) T->type->as_string(T);
                assert(String_isString((Object*) S));
                printf(" (%.*s)", S->length, S->characters);
            }
        }
    }

    default:
        break;
    }
    printf("\n");
}

void
print_instructions(const CodeContext *context, const Instruction *block, int count) {
    static bool sorted = false;
    if (!sorted) {
        qsort(OpcodeNames, sizeof(OpcodeNames) / sizeof(struct named_opcode),
            sizeof(struct named_opcode), cmpfunc);
    }

    while (count--) {
        print_opcode(context, block++);
    }
}

void
print_codeblock(const CodeContext* context, const CodeBlock *block) {
    print_instructions(context, block->instructions, block->nInstructions);
}