#include <assert.h>

#include "vm.h"
#include "compile.h"
#include "Include/Lox.h"

#define BINARY_METHOD(method) do { rhs = POP(eval); lhs = POP(eval); PUSH(eval, (Object*) lhs->type->method(lhs, rhs)); } while(0)

static Object*
vmeval_eval(EvalContext* eval, CodeContext* context) {
    Object *lhs, *rhs;
    HashObject *locals = Hash_new();

    eval->pc = context->block->instructions;
    Instruction *end = eval->pc + context->block->nInstructions, *instruction;

    // Return NIL if stack is empty
    PUSH(eval, LoxNIL);

    while (eval->pc < end) {
        instruction = eval->pc;
//        print_instructions(context, instruction, 1);
        switch (instruction->op) {
            case OP_JUMP:
            JUMP(eval, instruction->arg);
            break;

            case OP_POP_JUMP_IF_TRUE:
            lhs = POP(eval);
            if (Bool_isTrue(lhs))
                JUMP(eval, instruction->arg);
            break;

            case OP_POP_JUMP_IF_FALSE:
            lhs = POP(eval);
            if (!Bool_isTrue(lhs))
                JUMP(eval, instruction->arg);
            break;

            case OP_DUP_TOP:
            PUSH(eval, PEEK(eval));
            break;

            case OP_CALL:
            case OP_RETURN:
            break;

            case OP_LOOKUP:
            lhs = *(context->constants + instruction->arg);
            PUSH(eval, Hash_getItem(locals, lhs));
            break;

            case OP_STORE:
            lhs = *(context->constants + instruction->arg);
            Hash_setItem(locals, lhs, POP(eval));
            break;

            case OP_STORE_LOCAL:
            assert(instruction->arg < context->locals.count);
            *(eval->locals + instruction->arg) = POP(eval);
            break;

            case OP_LOOKUP_LOCAL:
            assert(instruction->arg < context->locals.count);
            lhs = *(eval->locals + instruction->arg);
            if (lhs == NULL) {
                // RAISE RUNTIME ERROR
                fprintf(stderr, "WARNING: `%hd` accessed before assignment", pc->arg);
                lhs = LoxNIL;
            }
            PUSH(eval, lhs);
            break;

            case OP_CONSTANT:
            lhs = *(context->constants + instruction->arg);
            PUSH(eval, lhs);
            break;

            // Comparison
            case OP_GT:
            BINARY_METHOD(op_gt);

            break;

            case OP_GTE:
            BINARY_METHOD(op_gte);
            break;

            case OP_LT:
            BINARY_METHOD(op_lt);
            break;

            case OP_LTE:
            BINARY_METHOD(op_lte);
            break;

            case OP_EQUAL:
            BINARY_METHOD(op_eq);
            break;

            case OP_NEQ:

            // Boolean
            case OP_BINARY_AND:
            case OP_BINARY_OR:
            case OP_BANG:

            // Expressions
            case OP_BINARY_PLUS:
            BINARY_METHOD(op_plus);

            break;

            case OP_BINARY_MINUS:
            BINARY_METHOD(op_minus);
            break;

            case OP_BINARY_STAR:
            BINARY_METHOD(op_star);
            break;

            case OP_BINARY_SLASH:
            BINARY_METHOD(op_slash);
            break;

            default:
            printf("Unexpected OPCODE");
            case OP_NOOP:
            break;
        }
        eval->pc++;
    }

    int i = context->locals.count - 1;
    if (i > -1)
        for (; i; i--)
            DECREF(*(eval->locals + i));

    DECREF(locals);
    return POP(eval);
}

void
vmeval_init(EvalContext *eval, CodeContext* context) {
    Object **stack = calloc(32, sizeof(Object*));
    *eval = (EvalContext) {
        .stack = stack,
        .stack_size = 32,
        .pc = 0,
        .locals = calloc(context->locals.count, sizeof(Object*)),
    };
}

Object*
vmeval_string(const char * text, size_t length) {
    Compiler compiler;
    CodeContext *context;
    
    context = compile_string(&compiler, text, length);

    print_codeblock(context, context->block);

    EvalContext self;
    vmeval_init(&self, context);
    return vmeval_eval(&self, context);
}