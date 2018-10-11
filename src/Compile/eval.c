#include <assert.h>
#include <string.h>

#include "vm.h"
#include "compile.h"
#include "Include/Lox.h"
#include "Lib/builtin.h"
#include "Vendor/bdwgc/include/gc.h"

#include "Objects/hash.h"
#include "Objects/class.h"

#define STACK_SIZE 32

Object*
vmeval_eval(VmEvalContext *ctx) {
    Object *lhs, *rhs, **local, *rv;
    Constant *C;

    // Setup storage for the stack and locals
    Object *_locals[ctx->code->locals.count], **locals = &_locals[0];
    memset(&_locals, 0, sizeof(_locals));

    // TODO: Add estimate for MAX_STACK in the compile phase
    // XXX: Program could overflow 32-slot stack
    Object *_stack[STACK_SIZE], **stack = &_stack[0];

    Instruction *pc = ctx->code->block->instructions;
    Instruction *end = pc + ctx->code->block->nInstructions;

    // Default result is NIL
    PUSH(stack, LoxNIL);

    int i;

    while (pc < end) {
#if DEBUG
        print_instructions(ctx->code, pc, 1);
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

            case OP_JUMP_IF_TRUE:
            lhs = PEEK(stack);
            if (Bool_isTrue(lhs))
                pc += pc->arg;
            break;

            case OP_POP_JUMP_IF_FALSE:
            lhs = POP(stack);
            if (!Bool_isTrue(lhs))
                pc += pc->arg;
            break;

            case OP_JUMP_IF_FALSE:
            lhs = PEEK(stack);
            if (!Bool_isTrue(lhs))
                pc += pc->arg;
            break;

            case OP_DUP_TOP:
            lhs = PEEK(stack);
            PUSH(stack, lhs);
            break;

            case OP_POP_TOP:
            POP(stack);
            break;

            case OP_CLOSE_FUN: {
                CodeObject *code = (CodeObject*) POP(stack);
                // Copy the locals into a persistent storage
                Object **closed_locals = GC_MALLOC(ctx->code->locals.count * sizeof(Object*));
                memcpy((void*) closed_locals, locals, sizeof(_locals));
                VmFunction *fun = CodeObject_makeFunction((Object*) code,
                    // XXX: Globals?
                    VmScope_create(ctx->scope, closed_locals, ctx->code));
                PUSH(stack, (Object*) fun);
            }
            break;

            case OP_CALL_FUN: {
                Object *fun = *(stack - pc->arg - 1);
                assert(Function_isCallable(fun));
                
                if (CodeObject_isCodeObject(fun)) {
                    VmEvalContext call_ctx = (VmEvalContext) {
                        .code = ((VmFunction*)fun)->code->code,
                        .scope = ((VmFunction*)fun)->scope,
                        .args = (VmCallArgs) {
                            .values = stack - pc->arg,
                            .count = pc->arg,
                        },
                    };
                    rv = vmeval_eval(&call_ctx);
                }
                else {
                    TupleObject *args = Tuple_fromList(pc->arg, stack - pc->arg);
                    rv = fun->type->call(fun, ctx->scope, NULL, (Object*) args);
                }

                stack -= pc->arg + 1; // POP_N, {pc->arg}
                PUSH(stack, rv);
            }
            break;

            case OP_RETURN:
            goto op_return;
            break;

            case OP_BUILD_CLASS: {
                size_t count = pc->arg;
                HashObject *attributes = Hash_newWithSize(pc->arg);
                while (count--) {
                    lhs = POP(stack);
                    Hash_setItem(attributes, lhs, POP(stack));
                }
                PUSH(stack, (Object*) Class_build(attributes));
            }
            break;
            
            case OP_GET_ATTR:
            C = ctx->code->constants + pc->arg;
            lhs = POP(stack);
            PUSH(stack, lhs->type->getattr(lhs, C->value));
            break;

            case OP_LOOKUP:
            C = ctx->code->constants + pc->arg;
            assert(ctx->scope);
            PUSH(stack, VmScope_lookup(ctx->scope, C->value, C->hash));
            break;

            case OP_STORE:
            C = ctx->code->constants + pc->arg;
            assert(ctx->scope);
            VmScope_assign(ctx->scope, C->value, POP(stack), C->hash);
            break;

            case OP_STORE_LOCAL:
            assert(pc->arg < ctx->code->locals.count);
            lhs = *(locals + pc->arg);
            *(locals + pc->arg) = POP(stack);
            break;

            case OP_STORE_ARG_LOCAL:
            assert(ctx->args.count);
            *(locals + pc->arg) = *(ctx->args.values++);
            ctx->args.count--;
            break;

            case OP_LOOKUP_LOCAL:
            assert(pc->arg < ctx->code->locals.count);
            lhs = *(locals + pc->arg);
            if (lhs == NULL) {
                // RAISE RUNTIME ERROR
                fprintf(stderr, "WARNING: `%hd` accessed before assignment", pc->arg);
                lhs = LoxNIL;
            }
            PUSH(stack, lhs);
            break;

            case OP_CONSTANT:
            C = ctx->code->constants + pc->arg;
            PUSH(stack, C->value);
            break;

#define BINARY_METHOD(method) do { rhs = POP(stack); lhs = POP(stack); PUSH(stack, (Object*) lhs->type->method(lhs, rhs)); } while(0)

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
            printf("Unexpected OPCODE (%d)\n", pc->op);
            case OP_NOOP:
            break;
        }
        pc++;
    }

op_return:
    rv = POP(stack);

    assert(stack >= _stack);
    assert(stack - _stack < STACK_SIZE);

    return rv;
}

static Object*
vmeval_inscope(CodeContext *code, VmScope *scope) {
    static ModuleObject* builtins = NULL;
    if (builtins == NULL)
        builtins = BuiltinModule_init();

    VmScope superglobals = (VmScope) {
        .globals = builtins->properties,
    };

    VmScope module = (VmScope) {
        .globals = Hash_new(),
        .outer = &superglobals,
    };
 
    VmScope final;
    if (scope) {
        final = *scope;
        final.outer = &module;
    }
    else {
        final = module;
    }

    VmEvalContext ctx = (VmEvalContext) {
        .code = code,
        .scope = &final,
    };

    return vmeval_eval(&ctx);
}

Object*
vmeval_string_inscope(const char * text, size_t length, VmScope* scope) {
    Compiler compiler = { .flags = 0 };
    CodeContext *context;
    
    context = compile_string(&compiler, text, length);

    print_codeblock(context, context->block);
    printf("------\n");
    
    return vmeval_inscope(context, scope);
}

Object*
vmeval_string(const char * text, size_t length) {
    return vmeval_string_inscope(text, length, NULL);
}

Object*
vmeval_file(FILE *input) {
    Compiler compiler = { .flags = 0 };
    CodeContext *context;
    
    context = compile_file(&compiler, input);

    print_codeblock(context, context->block);

    return vmeval_inscope(context, NULL);
}