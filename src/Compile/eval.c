#include <assert.h>

#include "vm.h"
#include "compile.h"
#include "Include/Lox.h"

#define BINARY_METHOD(method) do { rhs = POP(stack); lhs = POP(stack); PUSH(stack, (Object*) lhs->type->method(lhs, rhs)); } while(0)

Object*
vmeval_eval(CodeContext *context, VmScope *scope, VmCallArgs *args) {
    Object *lhs, *rhs, **local;
    Constant *C;

    // Setup storage for the stack and locals
    Object *_locals[context->locals.count], **locals = &_locals[0];

    // TODO: Add estimate for MAX_STACK in the compile phase
    // XXX: Program could overflow 32-slot stack
    static Object *_stack[32], **stack = &_stack[0];

    Instruction *pc = context->block->instructions;
    Instruction *end = pc + context->block->nInstructions;

    int i;

    while (pc < end) {
#if DEBUG
        print_instructions(context, instruction, 1);
#endif
        switch (pc->op) {
            case OP_JUMP:
            pc += pc->arg;
            break;

            case OP_POP_JUMP_IF_TRUE:
            lhs = POP(stack);
            if (Bool_isTrue(lhs))
                pc += pc->arg;
            break;

            case OP_POP_JUMP_IF_FALSE:
            lhs = POP(stack);
            if (!Bool_isTrue(lhs))
                pc += pc->arg;
            break;

            case OP_DUP_TOP:
            lhs = PEEK(stack);
            PUSH(stack, lhs);
            break;

            case OP_MAKE_FUN: {
                CodeObject *code = POP(stack);
                VmFunction *fun = CodeObject_makeFunction(code,
                    // XXX: Globals?
                    VmScope_create(scope, locals, context));
                PUSH(stack, (Object*) fun);
            }
            break;

            case OP_CALL_FUN: {
                VmFunction *fun = POP(stack);
                VmCallArgs args = (VmCallArgs) {
                    .values = stack - pc->arg,
                    .count = pc->arg,
                };
                Object *rv = vmeval_eval(fun->code->code, fun->scope, &args);
                stack -= pc->arg; // POP_N, {pc->arg}
                PUSH(stack, rv);
            }
            break;

            case OP_RETURN:
            goto op_return;
            break;

            case OP_LOOKUP:
            C = context->constants + pc->arg;
            assert(scope);
            PUSH(stack, VmScope_lookup(scope, C->value));
            break;

            case OP_STORE:
            C = context->constants + pc->arg;
            assert(scope);
            VmScope_assign(scope, C->value, POP(stack));
            break;

            case OP_STORE_LOCAL:
            assert(pc->arg < context->locals.count);
            *(locals + pc->arg) = POP(stack);
            break;

            case OP_STORE_ARG_LOCAL:
            assert(args);
            assert(args->count);
            *(locals + pc->arg) = *args->values++;
            args->count--;
            break;

            case OP_LOOKUP_LOCAL:
            assert(pc->arg < context->locals.count);
            lhs = *(locals + pc->arg);
            if (lhs == NULL) {
                // RAISE RUNTIME ERROR
                fprintf(stderr, "WARNING: `%hd` accessed before assignment", pc->arg);
                lhs = LoxNIL;
            }
            PUSH(stack, lhs);
            break;

            case OP_CONSTANT:
            C = context->constants + pc->arg;
            PUSH(stack, C->value);
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
        pc++;
    }

op_return:
    // Garbage collect local vars
    i = context->locals.count;
    while (i--)
        if (*(locals + i))
            DECREF(*(locals + i));

    return POP(stack);
}

Object*
vmeval_string(const char * text, size_t length) {
    Compiler compiler;
    CodeContext *context;
    
    context = compile_string(&compiler, text, length);

    print_codeblock(context, context->block);

    return vmeval_eval(context, NULL, NULL);
}