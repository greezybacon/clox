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

static void
eval_raise_error(VmEvalContext *ctx, const char *characters, ...) {
    // TODO: Make the characters a real Exception object
}

Object*
vmeval_eval(VmEvalContext *ctx) {
    Object *lhs, *rhs, **local, *rv, *item;
    Constant *C;

    Object *_locals[ctx->code->locals.count], **locals = &_locals[0];
    bool locals_in_stack = true;

    // Store parameters in the local variables
    int i = ctx->args.count;
    assert(ctx->code->locals.count >= ctx->args.count);
    while (i--) {
        *(locals + i) = *(ctx->args.values + i);
		INCREF(*(locals + i));
	}

    i = ctx->code->locals.count - ctx->args.count;
    while (i--) {
        *(locals + i) = LoxUndefined;
		INCREF(LoxUndefined);
	}

    // TODO: Add estimate for MAX_STACK in the compile phase
    // XXX: Program could overflow 32-slot stack
    Object *_stack[STACK_SIZE], **stack = &_stack[0];

    Instruction *pc = ctx->code->block->instructions;
    Instruction *end = pc + ctx->code->block->nInstructions;

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
            DECREF(lhs);
            break;

        case OP_JUMP_IF_TRUE:
            lhs = PEEK(stack);
            if (Bool_isTrue(lhs))
                pc += pc->arg;
            DECREF(lhs);
            break;

        case OP_POP_JUMP_IF_FALSE:
            lhs = POP(stack);
            if (!Bool_isTrue(lhs))
                pc += pc->arg;
            DECREF(lhs);
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
            XPOP(stack);
            break;

        case OP_CLOSE_FUN: {
            CodeObject *code = (CodeObject*) POP(stack);
            // Move the locals into a malloc'd object so that future local
            // changes will be represented in this closure
            if (locals_in_stack) {
                unsigned locals_count = ctx->code->locals.count;
                Object **mlocals = GC_MALLOC(sizeof(Object*) * locals_count);
                while (locals_count--) {
                    *(mlocals + locals_count) = *(locals + locals_count);
                    INCREF(*(mlocals + locals_count));
                }
                locals = mlocals;
                locals_in_stack = false;
            }
            VmFunction *fun = CodeObject_makeFunction((Object*) code,
                // XXX: Globals?
                VmScope_create(ctx->scope, code->context, locals, ctx->code->locals.count));
            PUSH(stack, (Object*) fun);
            break;
        }

        case OP_CALL_FUN: {
            Object *fun = *(stack - pc->arg - 1);

            if (VmFunction_isVmFunction(fun)) {
                VmEvalContext call_ctx = (VmEvalContext) {
                    .code = ((VmFunction*)fun)->code->context,
                    .scope = ((VmFunction*)fun)->scope,
                    .args = (VmCallArgs) {
                        .values = stack - pc->arg,
                        .count = pc->arg,
                    },
                };
                rv = vmeval_eval(&call_ctx);
            }
            else if (Function_isCallable(fun)) {
                TupleObject *args = Tuple_fromList(pc->arg, stack - pc->arg);
                rv = fun->type->call(fun, ctx->scope, ctx->this, (Object*) args);
                INCREF(rv);
                LoxObject_Cleanup((Object*) args);
            }
            else {
                fprintf(stderr, "WARNING: Type `%s` is not callable\n", fun->type->name);
                rv = LoxUndefined;
            }

            i = pc->arg; // POP_N, {pc->arg}
            XPOP(stack);
            while (i--)
                XPOP(stack);

            XPUSH(stack, rv);
            break;
        }

        case OP_RECURSE: {
            // This will only happen for a VmFunction. In this case, we will
            // execute the same code again, but with different arguments.
            VmEvalContext call_ctx = *ctx;
            call_ctx.args = (VmCallArgs) {
                .values = stack - pc->arg,
                .count = pc->arg,
            };
            // Don't INCREF because the return value of the call should be
            // decref'd in this scope which would cancel an INCREF
            rv = vmeval_eval(&call_ctx);

            // DECREF all the arguments
            i = pc->arg;
            while (i--)
                XPOP(stack);

            XPUSH(stack, rv);
            break;
        }

        case OP_RETURN:
            goto op_return;
            break;

        case OP_BUILD_SUBCLASS:
            rhs = POP(stack);               // (parent)
            // Build the class as usual
        case OP_BUILD_CLASS: {
            size_t count = pc->arg;
            HashObject *attributes = Hash_newWithSize(count);
            while (count--) {
                lhs = POP(stack);
                rhs = POP(stack);
                Hash_setItem(attributes, lhs, rhs);
                DECREF(lhs);
                DECREF(rhs);
            }
            PUSH(stack, (Object*) Class_build(attributes,
                pc->op == OP_BUILD_SUBCLASS ? (ClassObject*) rhs : NULL));
            if (pc->op == OP_BUILD_SUBCLASS)
                DECREF(rhs);
            break;
        }

        case OP_GET_ATTR:
            C = ctx->code->constants + pc->arg;
            lhs = POP(stack);
            PUSH(stack, object_getattr(lhs, C->value));
            DECREF(lhs);
            break;

        case OP_SET_ATTR:
            C = ctx->code->constants + pc->arg;
            rhs = POP(stack);
            lhs = POP(stack);
            if (unlikely(!lhs->type->setattr)) {
                fprintf(stderr, "WARNING: `setattr` not defined for type: `%s`\n", lhs->type->name);
            }
            else {
                lhs->type->setattr(lhs, C->value, rhs);
            }
            DECREF(rhs);
            DECREF(lhs);
            break;

        case OP_THIS:
            PUSH(stack, ctx->this);
            break;

        case OP_SUPER: {
            lhs = (Object*) ctx->code->owner;
            if (unlikely(!lhs)) {
                fprintf(stderr, "WARNING: `super` used in non-class method\n");
                lhs = LoxUndefined;
            }
            else {
                assert(Class_isClass(lhs));
                lhs = (Object*) ((ClassObject*) lhs)->parent;
                if (unlikely(!lhs)) {
                    fprintf(stderr, "WARNING: `super` can only be used in a subclass\n");
                    lhs = LoxUndefined;
                }
            }
            PUSH(stack, lhs);
            break;
        }

        case OP_LOOKUP:
        case OP_LOOKUP_GLOBAL:
            C = ctx->code->constants + pc->arg;
            assert(ctx->scope);
            PUSH(stack, VmScope_lookup_global(ctx->scope, C->value, C->hash));
            break;

        case OP_LOOKUP_CLOSED:
            if (ctx->scope) {
                lhs = VmScope_lookup_local(ctx->scope, pc->arg);
            }
            else
                lhs = LoxUndefined;
            PUSH(stack, lhs);
            break;

        case OP_STORE:
        case OP_STORE_GLOBAL:
            C = ctx->code->constants + pc->arg;
            assert(ctx->scope);
            lhs = POP(stack);
            VmScope_assign(ctx->scope, C->value, lhs, C->hash);
            DECREF(lhs);
            break;

        case OP_STORE_LOCAL:
            assert(pc->arg < ctx->code->locals.count);
            item = *(locals + pc->arg);
            if (item)
                DECREF(item);
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
                fprintf(stderr, "WARNING: `%.*s` (%hd) accessed before assignment",
                    ((StringObject*) (ctx->code->locals.names + pc->arg)->value)->length,
                    ((StringObject*) (ctx->code->locals.names + pc->arg)->value)->characters,
                    pc->arg);
                lhs = LoxUndefined;
            }
            PUSH(stack, lhs);
            break;

        case OP_CONSTANT:
            C = ctx->code->constants + pc->arg;
            PUSH(stack, C->value);
            break;

#define BINARY_METHOD(method) do { rhs = POP(stack); lhs = POP(stack); PUSH(stack, (Object*) lhs->type->method(lhs, rhs)); DECREF(lhs); DECREF(rhs); } while(0)

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
            break;

        case OP_BANG:
            lhs = POP(stack);
            PUSH(stack, (Object*) (Bool_fromObject(lhs) == LoxTRUE ? LoxFALSE : LoxTRUE));
            DECREF(lhs);
            break;

        case OP_IN:
            lhs = POP(stack);
            rhs = POP(stack);
            if (likely(lhs->type->contains != NULL)) {
                PUSH(stack, (Object*) lhs->type->contains(lhs, rhs));
            }
            else {
                fprintf(stderr, "WARNING: Type <%s> does not support IN\n", lhs->type->name);
            }
            DECREF(lhs);
            DECREF(rhs);
            break;

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

        case OP_NEG:
            lhs = POP(stack);
            PUSH(stack, (Object*) lhs->type->op_neg(lhs));
            DECREF(lhs);
            break;

        case OP_GET_ITEM:
            rhs = POP(stack);
            lhs = POP(stack);
            if (lhs->type->get_item)
                PUSH(stack, lhs->type->get_item(lhs, rhs));
            else
                fprintf(stderr, "lhs type `%s` does not support GET_ITEM\n", lhs->type->name);
            DECREF(lhs);
            DECREF(rhs);
            break;

        case OP_SET_ITEM:
            rhs = POP(stack);
            item = POP(stack);
            lhs = POP(stack);
            if (lhs->type->set_item)
                lhs->type->set_item(lhs, item, rhs);
            else
                fprintf(stderr, "lhs type `%s` does not support SET_ITEM\n", lhs->type->name);
            DECREF(lhs);
            DECREF(rhs);
            DECREF(item);
            break;

        case OP_BUILD_TUPLE:
            item = Tuple_fromList(pc->arg, stack - pc->arg);
            i = pc->arg;
            while (i--)
                XPOP(stack);
            PUSH(stack, item);
            break;

        default:
            printf("Unexpected OPCODE (%d)\n", pc->op);
        case OP_NOOP:
            break;
        }
        pc++;
    }

op_return:
    // Default return value is NIL
    if (stack == _stack)
        rv = LoxNIL;
    else
        rv = POP(stack);

    i = ctx->code->locals.count;
    while (i--)
        DECREF(*(locals + i));

    // Check stack overflow and underflow
    assert(stack == _stack);
    //assert(stack - _stack < STACK_SIZE);

    return rv;
}

static Object*
vmeval_inscope(CodeContext *code, VmScope *scope) {
    static ModuleObject* builtins = NULL;
    if (builtins == NULL) {
        builtins = BuiltinModule_init();
        INCREF((Object*) builtins);
    }

    VmScope superglobals = (VmScope) {
        .globals = builtins->properties,
    };

    VmScope final;
    if (scope) {
        final = *scope;
    }
    else {
        final = (VmScope) {
            .globals = Hash_new(),
        };
    }
    final.outer = &superglobals;

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