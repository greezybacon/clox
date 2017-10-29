#ifndef VM_H
#define VM_H

enum vm_opcode {
    OP_NOOP=0,          // Skip to next instruction
    OP_JUMP_GT,         // IF R1 > R2, JUMP-> R3
    OP_JUMP_GTE,
    OP_JUMP_LT,
    OP_JUMP_LTE,
    OP_JUMP_FALSE,
    OP_JUMP_TRUE,
    OP_BINARY_ADD,      // ADD R1, R2, => R3
    OP_BINARY_SUB,
    OP_BINARY_MUL,
    OP_BINARY_DIV,
};

enum vm_register {
    R1=0,
};

typedef struct vm_frame {
    enum vm_opcode  op;
} VmFrame;

typedef struct vm_binary {
    VmFrame             frame;
    enum vm_register    lhs;
    enum vm_register    rhs;
    enum vm_register    result;
} VMFrameBinary;

#endif