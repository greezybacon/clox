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

static inline void
store_arg_indirect(VmEvalContext *ctx, unsigned index,
    enum op_var_location_type location, Object *value
) {
    Constant *C;

    switch (location) {
    case OP_VAR_LOCATE_REGISTER:
        ctx->regs[index] = value;
        break;
    case OP_VAR_LOCATE_CLOSURE:
        fprintf(stderr, "Warning: Non-local writes are not yet supported\n");
        break;
    case OP_VAR_LOCATE_CONSTANT:
        fprintf(stderr, "Error: Cannot write to constant\n");
        break;
    case OP_VAR_LOCATE_GLOBAL:
        C = ctx->code->constants + index;
        VmScope_assign(ctx->scope, C->value, value, C->hash);
    }
}

static inline Object*
fetch_arg_indirect(
    VmEvalContext *ctx, unsigned index, enum op_var_location_type location
) {
    Constant *C;

    switch (location) {
    case OP_VAR_LOCATE_REGISTER:
        return ctx->regs[index];
    case OP_VAR_LOCATE_CLOSURE:
        return VmScope_lookup_local(ctx->scope, index);
    case OP_VAR_LOCATE_CONSTANT:
        return (ctx->code->constants + index)->value;
    case OP_VAR_LOCATE_GLOBAL:
        C = ctx->code->constants + index;
        return VmScope_lookup_global(ctx->scope, C->value, C->hash);
    }
}

Object*
vmeval_eval(VmEvalContext *ctx) {
    Object *lhs, *rhs, **local, *rv = LoxUndefined, *out;
    Constant *C;

    Object *_regs[ctx->code->regs_required];
    ctx->regs = &_regs[0];

    // Store parameters in the local variables
    int i = ctx->code->locals.count, j = ctx->args.count;
    assert(i >= j);
    while (i > j) {
        *(ctx->regs + (ctx->code->locals.vars + i++)->regnum) = LoxUndefined;
		INCREF(LoxUndefined);
	}

    while (i--) {
        _regs[i] = *(ctx->args.values + i);
		INCREF(_regs[i]);
	}

    Instruction *pc = ctx->code->block->instructions;
    Instruction *end = ((void*)pc) + ctx->code->block->bytes;

    while (pc < end) {
#if DEBUG
        print_instructions(ctx->code, pc, 1);
#endif
        switch (pc->opcode) {
        case ROP_STORE: {
            ShortInstruction *si = (ShortInstruction*) pc;
            store_arg_indirect(ctx, si->p1, si->flags.lro.lhs,
                fetch_arg_indirect(ctx, si->p2, si->flags.lro.rhs));
            pc = ((void*) pc) + ROP_STORE__LEN;
            break;
        }

        case ROP_MATH: {
            lhs = fetch_arg_indirect(ctx, pc->p1, pc->flags.lro.lhs);
            rhs = fetch_arg_indirect(ctx, pc->p2, pc->flags.lro.rhs);

            // The pc->subtype is the binary function index in the object type.
            // The first on is op_plus. We should just advance ahead a number
            // of functions based on pc->arg to find the appropriate method
            // to invoke.
            lox_vm_binary_math_func *operfunc =
                (void*) lhs->type
                + offsetof(ObjectType, op_plus)
                + pc->subtype * sizeof(lox_vm_binary_math_func);

            if (*operfunc) {
                assert(pc->subtype < __MATH_BINARY_MAX);
                out = (*operfunc)(lhs, rhs);
            }
            else {
                fprintf(stderr, "WARNING: Type `%s` does not support op `%hhu`\n", lhs->type->name, pc->subtype);
                out = LoxUndefined;
            }
            store_arg_indirect(ctx, pc->p3, pc->flags.lro.out, out);
            pc = ((void*) pc) + ROP_MATH__LEN;
            break;
        }

        case ROP_COMPARE: {
            lhs = fetch_arg_indirect(ctx, pc->p1, pc->flags.lro.lhs);
            rhs = fetch_arg_indirect(ctx, pc->p2, pc->flags.lro.rhs);

            int comparison;
            if (pc->subtype == COMPARE_IN) {
                ;
            }
            else if (lhs->type->compare) {
                comparison = lhs->type->compare(lhs, rhs);
            }
            else if (rhs->type->compare) {
                comparison = - rhs->type->compare(rhs, lhs);
            }
            else {
                printf("WARNING: Types `%s` and `%s` are not comparable", lhs->type->name,
                    rhs->type->name);
                comparison = -1;
            }

            switch ((enum lox_vm_compare) pc->subtype) {
                case COMPARE_EQ:
                    store_arg_indirect(ctx, pc->p3, pc->flags.lro.out,
                         (Object*) (comparison == 0 ? LoxTRUE : LoxFALSE));
                    break;
                case COMPARE_NOT_EQ:
                    store_arg_indirect(ctx, pc->p3, pc->flags.lro.out,
                         (Object*) (comparison != 0 ? LoxTRUE : LoxFALSE));
                    break;
                case COMPARE_LT:
                    store_arg_indirect(ctx, pc->p3, pc->flags.lro.out,
                         (Object*) (comparison < 0 ? LoxTRUE : LoxFALSE));
                    break;
                case COMPARE_LTE:
                    store_arg_indirect(ctx, pc->p3, pc->flags.lro.out,
                         (Object*) (comparison > 0 ? LoxFALSE : LoxTRUE));
                    break;
                case COMPARE_GT:
                    store_arg_indirect(ctx, pc->p3, pc->flags.lro.out,
                         (Object*) (comparison > 0 ? LoxTRUE : LoxFALSE));
                    break;
                case COMPARE_GTE:
                    store_arg_indirect(ctx, pc->p3, pc->flags.lro.out,
                         (Object*) (comparison < 0 ? LoxFALSE : LoxTRUE));
                    break;
                case COMPARE_IN:
                    if (likely(lhs->type->contains != NULL)) {
                        out = (Object*) lhs->type->contains(lhs, rhs);
                    }
                    else {
                        fprintf(stderr, "WARNING: Type <%s> does not support IN\n", lhs->type->name);
                        out = LoxUndefined;
                    }
                    store_arg_indirect(ctx, pc->p3, pc->flags.lro.out, out);
                    break;
                case COMPARE_EXACT:
                case COMPARE_NOT_EXACT:
                case COMPARE_IS:
                case COMPARE_SPACESHIP:
                    break;
            }
            pc = ((void*) pc) + ROP_COMPARE__LEN;
            break;
        }

        case ROP_CONTROL: {
            switch (pc->subtype) {
            case OP_CONTROL_JUMP_IF_TRUE:
                if (Bool_isTrue(fetch_arg_indirect(ctx, pc->p1, pc->flags.lro.lhs)))
                    pc = ((void*) pc) + pc->p23;
                break;

            case OP_CONTROL_JUMP_IF_FALSE:
                if (!Bool_isTrue(fetch_arg_indirect(ctx, pc->p1, pc->flags.lro.lhs)))
                    pc = ((void*) pc) + pc->p23;
                break;

            case OP_CONTROL_JUMP:
                pc = ((void*) pc) + pc->p23;
                break;

            case OP_CONTROL_RETURN:
                rv = fetch_arg_indirect(ctx, pc->p1, pc->flags.lro.lhs);
                goto op_return;

            case OP_CONTROL_LOOP_SETUP:
            case OP_CONTROL_LOOP_BREAK:
            case OP_CONTROL_LOOP_CONTINUE:
                break;
            }

            pc = ((void*) pc) + ROP_CONTROL__LEN;
            break;
        }

        case ROP_CALL: {
            lhs = fetch_arg_indirect(ctx, pc->subtype, pc->flags.lro.rhs);

            if (VmFunction_isVmFunction(lhs)) {
                Object *args[pc->len];
                unsigned count = pc->len, i = 0;
                const ShortArg *A = pc->args;

                while (count--) {
                    // TODO: Handle long arguments
                    args[i++] = fetch_arg_indirect(ctx, A->index, A->location);
                    A++;
                }
                VmEvalContext call_ctx = (VmEvalContext) {
                    .code = ((LoxVmFunction*)lhs)->code->context,
                    .scope = ((LoxVmFunction*)lhs)->scope,
                    .args = (VmCallArgs) {
                        .values = args,
                        .count = pc->len,
                    },
                };
                rv = vmeval_eval(&call_ctx);
            }
            else if (Function_isCallable(lhs)) {
                LoxTuple *args = Tuple_new(pc->len);
                unsigned count = pc->len, i = 0;
                const ShortArg *A = pc->args;

                while (count--) {
                    // TODO: Handle long arguments
                    Tuple_SETITEM(args, i++, fetch_arg_indirect(ctx, A->index, A->location));
                    A++;
                }
                out = lhs->type->call(lhs, ctx->scope, ctx->this, (Object*) args);
                store_arg_indirect(ctx, pc->p1, pc->flags.lro.out, out);
                LoxObject_Cleanup((Object*) args);
            }
            else {
                fprintf(stderr, "WARNING: Type `%s` is not callable\n", lhs->type->name);
                rv = LoxUndefined;
            }
            pc = ((void*) pc) + ROP_CALL__LEN_BASE + pc->len;
            break;
        }

        case ROP_BUILD: {
            switch (pc->subtype) {
            case OP_BUILD_FUNCTION: {
                LoxVmCode *code = (LoxVmCode*) fetch_arg_indirect(ctx, pc->p2, pc->flags.lro.rhs);

                // Move the locals into a malloc'd object so that future local
                // changes will be represented in this closure
                /*
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
                */
                LoxVmFunction *fun = VmCode_makeFunction((Object*) code,
                    // XXX: Globals?
                    VmScope_create(ctx->scope, code->context, NULL, ctx->code->locals.count));
                store_arg_indirect(ctx, pc->p1, pc->flags.lro.out, (Object*) fun);
            }
            pc = ((void*) pc) + ROP_BUILD__LEN_BASE;
            break;
        }}

        // OLD OPCODES --------------------------------------
/*
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
            break;

        case OP_JUMP_IF_TRUE_OR_POP:
            lhs = PEEK(stack);
            if (Bool_isTrue(lhs))
                pc += pc->arg;
            else
                XPOP(stack);
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

        case OP_JUMP_IF_FALSE_OR_POP:
            lhs = PEEK(stack);
            if (!Bool_isTrue(lhs))
                pc += pc->arg;
            else
                XPOP(stack);
            break;

        case OP_DUP_TOP:
            lhs = PEEK(stack);
            PUSH(stack, lhs);
            break;

        case OP_POP_TOP:
            XPOP(stack);
            break;

        case OP_CLOSE_FUN: {
            LoxVmCode *code = (LoxVmCode*) POP(stack);
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
            LoxVmFunction *fun = VmCode_makeFunction((Object*) code,
                // XXX: Globals?
                VmScope_create(ctx->scope, code->context, locals, ctx->code->locals.count));
            PUSH(stack, (Object*) fun);
            break;
        }

        case OP_CALL_FUN: {
            Object *fun = *(stack - pc->arg - 1);

            if (VmFunction_isVmFunction(fun)) {
                VmEvalContext call_ctx = (VmEvalContext) {
                    .code = ((LoxVmFunction*)fun)->code->context,
                    .scope = ((LoxVmFunction*)fun)->scope,
                    .args = (VmCallArgs) {
                        .values = stack - pc->arg,
                        .count = pc->arg,
                    },
                };
                rv = vmeval_eval(&call_ctx);
            }
            else if (Function_isCallable(fun)) {
                LoxTuple *args = Tuple_fromList(pc->arg, stack - pc->arg);
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
            // This will only happen for a LoxVmFunction. In this case, we will
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
            LoxTable *attributes = Hash_newWithSize(count);
            while (count--) {
                lhs = POP(stack);
                rhs = POP(stack);
                Hash_setItem(attributes, lhs, rhs);
                DECREF(lhs);
                DECREF(rhs);
            }
            PUSH(stack, (Object*) Class_build(attributes,
                pc->opcode == OP_BUILD_SUBCLASS ? (LoxClass*) rhs : NULL));
            if (pc->opcode == OP_BUILD_SUBCLASS)
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
                lhs = (Object*) ((LoxClass*) lhs)->parent;
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
                fprintf(stderr, "WARNING: `%.*s` (%hhu) accessed before assignment",
                    ((LoxString*) ((ctx->code->locals.vars + pc->arg)->name.value))->length,
                    ((LoxString*) ((ctx->code->locals.vars + pc->arg)->name.value))->characters,
                    pc->arg);
                lhs = LoxUndefined;
            }
            PUSH(stack, lhs);
            break;

        case OP_CONSTANT:
            C = ctx->code->constants + pc->arg;
            PUSH(stack, C->value);
            break;

#define BINARY_METHOD(method) do { \
    rhs = POP(stack); \
    lhs = POP(stack); \
    if (lhs->type->method) { \
        PUSH(stack, (Object*) lhs->type->method(lhs, rhs)); \
    } \
    else { \
        fprintf(stderr, "WARNING: Type `%s` does not support op `" #method "`\n", lhs->type->name); \
        PUSH(stack, LoxUndefined); \
    } \
    DECREF(lhs); \
    DECREF(rhs); \
} while(0)
    
#define BINARY_COMPARE() ({ \
    rhs = POP(stack); \
    lhs = POP(stack); \
    int x = (lhs->type->compare) ? lhs->type->compare(lhs, rhs) : (lhs == rhs ? 0 : -1); \
    DECREF(lhs); \
    DECREF(rhs); \
    x; \
})

        // Comparison
        case OP_COMPARE:
            switch ((enum lox_vm_compare) pc->arg) {
            case COMPARE_IN:
                lhs = POP(stack);
                rhs = POP(stack);
                if (likely(lhs->type->contains != NULL)) {
                    PUSH(stack, (Object*) lhs->type->contains(lhs, rhs));
                }
                else {
                    fprintf(stderr, "WARNING: Type <%s> does not support IN\n", lhs->type->name);
                    PUSH(stack, LoxUndefined);
                }
                DECREF(lhs);
                DECREF(rhs);
                break;
                
            case COMPARE_EXACT:
                rhs = POP(stack);
                lhs = POP(stack);
                if (lhs == rhs)
                    PUSH(stack, (Object*) LoxTRUE);
                else if (lhs->type != rhs->type)
                    PUSH(stack, (Object*) LoxFALSE);
                else if (lhs->type->compare && lhs->type->compare(lhs, rhs) == 0)
                    PUSH(stack, (Object*) LoxTRUE);
                else
                    PUSH(stack, (Object*) LoxFALSE);
                DECREF(lhs); \
                DECREF(rhs); \
                break;

            case COMPARE_EQ:
                PUSH(stack, (Object*) (BINARY_COMPARE() == 0 ? LoxTRUE : LoxFALSE));
                break;
            case COMPARE_NOT_EQ:
                PUSH(stack, (Object*) (BINARY_COMPARE() != 0 ? LoxTRUE : LoxFALSE));
                break;
            case COMPARE_GT:
                PUSH(stack, (Object*) (BINARY_COMPARE() > 0 ? LoxTRUE : LoxFALSE));
                break;
            case COMPARE_GTE:
                PUSH(stack, (Object*) (BINARY_COMPARE() < 0 ? LoxFALSE : LoxTRUE));
                break;
            case COMPARE_LT:
                PUSH(stack, (Object*) (BINARY_COMPARE() < 0 ? LoxTRUE : LoxFALSE));
                break;
            case COMPARE_LTE:
                PUSH(stack, (Object*) (BINARY_COMPARE() > 0 ? LoxFALSE : LoxTRUE));
                break;
            default:
                fprintf(stderr, "WARNING: %hhu: Unimplemneted compare operation\n", pc->arg);
                PUSH(stack, LoxUndefined);
            }
            break;

        // Boolean
        case OP_BANG:
            lhs = POP(stack);
            PUSH(stack, (Object*) (Bool_isTrue(lhs) ? LoxFALSE : LoxTRUE));
            DECREF(lhs);
            break;

        // Expressions
        case OP_BINARY_MATH:
            rhs = POP(stack);
            lhs = POP(stack);

            // The pc->arg is the binary function index in the object type.
            // The first on is op_plus. We should just advance ahead a number
            // of functions based on pc->arg to find the appropriate method
            // to invoke.
            lox_vm_binary_math_func *operfunc = 
                (void*) lhs->type
                + offsetof(ObjectType, op_plus) 
                + pc->arg * sizeof(lox_vm_binary_math_func);

            if (*operfunc) {
                assert(pc->arg < __MATH_BINARY_MAX);
                PUSH(stack, (*operfunc)(lhs, rhs));
            }
            else {
                fprintf(stderr, "WARNING: Type `%s` does not support op `%hhu`\n", lhs->type->name, pc->arg);
                PUSH(stack, LoxUndefined);
            }
            DECREF(lhs);
            DECREF(rhs);
            break;

        case OP_UNARY_NEGATIVE:
            lhs = POP(stack);
            PUSH(stack, (Object*) lhs->type->op_neg(lhs));
            DECREF(lhs);
            break;

        case OP_UNARY_INVERT:
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
            item = (Object*) Tuple_fromList(pc->arg, stack - pc->arg);
            i = pc->arg;
            while (i--)
                XPOP(stack);
            PUSH(stack, item);
            break;

        case OP_BUILD_STRING:
            item = LoxString_BuildFromList(pc->arg, stack - pc->arg);
            i = pc->arg;
            while (i--)
                XPOP(stack);
            PUSH(stack, item);
            break;

        case OP_FORMAT:
            C = ctx->code->constants + pc->arg;
            assert(String_isString(C->value));
            item = LoxObject_Format(POP(stack), ((LoxString*) C->value)->characters);
            PUSH(stack, item);
            break;

        case OP_BUILD_TABLE:
            i = pc->arg;
            item = (Object*) Hash_newWithSize(i);
            while (i--) {
                rhs = POP(stack);
                lhs = POP(stack);
                Hash_setItem((LoxTable*) item, lhs, rhs);
                DECREF(rhs);
                DECREF(lhs);
            }
            PUSH(stack, item);
            break;

        default:
            printf("Unexpected OPCODE (%d)\n", pc->opcode);
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
*/
        }
    }

    if (ctx->code->result_reg != -1)
        rv = ctx->regs[ctx->code->result_reg];

op_return:
    return rv;
}

static Object*
vmeval_inscope(CodeContext *code, VmScope *scope) {
    static LoxModule* builtins = NULL;
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

Object*
LoxEval_EvalAST(ASTNode *input) {
    Compiler compiler = { .flags = 0 };
    CodeContext *context;

    context = compile_ast(&compiler, input);
    return vmeval_inscope(context, NULL);
}