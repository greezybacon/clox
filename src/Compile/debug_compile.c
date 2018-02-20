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
    { OP_DUP_TOP,       "DUP_TOP" },
    
    // Functions
    { OP_CALL,          "CALL" },
    { OP_RETURN,        "RETURN" },

    // Variables
    { OP_LOOKUP,        "LOOKUP" },
    { OP_STORE,         "STORE" },
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

    // Expressions / math
    { OP_BINARY_PLUS,   "PLUS (+)" },
    { OP_BINARY_MINUS,  "MINUS (-)" },
    { OP_BINARY_STAR,   "STAR (*)" },
    { OP_BINARY_SLASH,  "SLASH (/)" },
};

static int cmpfunc (const void * a, const void * b) {
   return ((struct named_opcode*) a)->code - ((struct named_opcode*) b)->code;
}

static inline void
print_opcode(CodeContext *context, Instruction *op) {
    struct named_opcode* T, key = { .code = op->op };
    T = bsearch(&key, OpcodeNames, sizeof(OpcodeNames) / sizeof(struct named_opcode), 
        sizeof(struct named_opcode), cmpfunc);

    printf("%-20s %d", T->name, op->arg);

    // For opcodes which use constants, print the constant value too
    switch (op->op) {
    case OP_CONSTANT:
    case OP_STORE:
    case OP_LOOKUP: {
        Object* T = *(context->constants + op->arg);
        if (T && T->type && T->type->as_string) {
            StringObject *S = T->type->as_string(T);
            printf(" (%.*s)", S->length, S->characters);
            if (!String_isString(T))
                DECREF(S);
        }
    }
    default:
        break;
    }
    printf("\n");
}

void
print_instructions(CodeContext *context, Instruction *block, int count) {
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
print_codeblock(CodeContext* context, CodeBlock *block) {
    print_instructions(context, block->instructions, block->nInstructions);
}