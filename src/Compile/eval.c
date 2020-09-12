#include <assert.h>
#include <string.h>

#include "vm.h"
#include "compile.h"
#include "Include/Lox.h"
#include "Lib/builtin.h"
#include "Vendor/bdwgc/include/gc.h"

#include "Objects/file.h"
#include "Objects/hash.h"
#include "Objects/class.h"
#include "Objects/exception.h"

#define STACK_SIZE 32

/**
 * Utility method to print the backtrace of the current execution stack.
 * Useful for debugging the interpreter.
 */
static void
vmeval_print_backtrace(VmEvalContext *ctx) {
    // Read through the codesources to identify the offset for the current opcode
    int offset = *ctx->pc - ctx->code->block->instructions.opcodes,
        cs_count = ctx->code->block->codesource.count;
    CodeSource *cs = ctx->code->block->codesource.offsets;

    while (cs_count && offset > cs->opcode_count) {
        offset -= cs->opcode_count;
        cs++;
        cs_count--;
    }

    if (ctx->previous)
        vmeval_print_backtrace(ctx->previous);

    printf("File \"%s\", on line %d\n",
        ctx->code->block->codesource.filename,
        cs->line_number);

    // TODO: Open target file and read to line x (skipping x-1 newlines)
}

static void
vmeval_raise(VmEvalContext *ctx, Object *exc) {
    assert(Exception_isException(exc));

    LoxString *S = String_fromObject((Object*) exc);
    INCREF(S);
    fprintf(stderr, "%.*s\n", S->length, S->characters);
    DECREF(S);

    vmeval_print_backtrace(ctx);

    exit(1);
}

static void
vmeval_raise_unhandled_error(VmEvalContext *ctx, const char *format, ...) {
    // TODO: Identify the source file and line number (which would require placing
    // that in the instruction block in the code context).
    // TODO: Make the characters a real Exception object
    va_list args;
    va_start(args, format);
    // LoxString *message = LoxObject_FormatWithScopeVa(ctx->scope, format, args);
    LoxString *message;
    Object* exc = Exception_fromFormatVa(format, args);
    va_end(args);

    vmeval_raise(ctx, exc);
}

static void
assert_safe_code(CodeContext *code) {
    const int locals_count = code->locals.count;

    Instruction *pc = code->block->instructions.opcodes;
    int count = code->block->instructions.count;
    while (count--) {
        switch (pc->op) {
        case OP_STORE_LOCAL:
        case OP_LOOKUP_LOCAL:
            assert(pc->arg < code->locals.count);
            break;
        case OP_BINARY_MATH:
            assert(pc->arg < __MATH_BINARY_MAX);
            break;
        default:
            break;
        }
        pc++;
    }
}

Object*
LoxVM_evalSafely(VmEvalContext *ctx) {
    assert_safe_code(ctx->code);
    return LoxVM_eval(ctx);
}

Object*
LoxVM_eval(VmEvalContext *ctx) {
    assert(ctx);
    assert(ctx->scope);

    Object *lhs, *rhs, **local, *rv = LoxUndefined, *item;
    Constant *C;

    Object *_locals[ctx->code->locals.count], **locals = &_locals[0];
    bool locals_in_stack = true;

    // Store parameters in the local variables
    int i = ctx->code->locals.count, j = ctx->args.count;
    assert(i >= j);
    while (i > j) {
        *(locals + --i) = LoxUndefined;
		INCREF(LoxUndefined);
	}

    while (i--) {
        *(locals + i) = *(ctx->args.values + i);
		INCREF(*(locals + i));
	}

    // TODO: Get number of nested block depth in compiler
    VmEvalLoopBlock blocks[8], *pblock = &blocks[0];

    // TODO: Add estimate for MAX_STACK in the compile phase
    // XXX: Program could overflow 32-slot stack
    Object *_stack[STACK_SIZE], **stack = &_stack[0];

    InstructionList *instructions = &ctx->code->block->instructions;
    Instruction *pc = instructions->opcodes,
        *end = pc + instructions->count;

    ctx->pc = &pc;

    static void *_labels[] = {
        [OP_NOOP] = &&OP_NOOP,
        [OP_HALT] = &&OP_HALT,
        [OP_JUMP] = &&OP_JUMP,
        [OP_JUMP_IF_TRUE] = &&OP_JUMP_IF_TRUE,
        [OP_POP_JUMP_IF_TRUE] = &&OP_POP_JUMP_IF_TRUE,
        [OP_JUMP_IF_FALSE] = &&OP_JUMP_IF_FALSE,
        [OP_POP_JUMP_IF_FALSE] = &&OP_POP_JUMP_IF_FALSE,
        [OP_JUMP_IF_FALSE_OR_POP] = &&OP_JUMP_IF_FALSE_OR_POP,
        [OP_JUMP_IF_TRUE_OR_POP] = &&OP_JUMP_IF_TRUE_OR_POP,
        [OP_DUP_TOP] = &&OP_DUP_TOP,
        [OP_POP_TOP] = &&OP_POP_TOP,
        [OP_CLOSE_FUN] = &&OP_CLOSE_FUN,
        [OP_CALL_FUN] = &&OP_CALL_FUN,
        [OP_RETURN] = &&OP_RETURN,
        [OP_RECURSE] = &&OP_RECURSE,
        [OP_LOOKUP_LOCAL] = &&OP_LOOKUP_LOCAL,
        [OP_LOOKUP_GLOBAL] = &&OP_LOOKUP_GLOBAL,
        [OP_LOOKUP_CLOSED] = &&OP_LOOKUP_CLOSED,
        [OP_STORE_LOCAL] = &&OP_STORE_LOCAL,
        [OP_STORE_GLOBAL] = &&OP_STORE_GLOBAL,
//        [OP_STORE_CLOSED] = &&OP_STORE_CLOSED,
        [OP_CONSTANT] = &&OP_CONSTANT,
        [OP_COMPARE] = &&OP_COMPARE,
        [OP_BANG] = &&OP_BANG,
        [OP_BINARY_MATH] = &&OP_BINARY_MATH,
        [OP_UNARY_NEGATIVE] = &&OP_UNARY_NEGATIVE,
        [OP_UNARY_INVERT] = &&OP_UNARY_INVERT,
        [OP_BUILD_CLASS] = &&OP_BUILD_CLASS,
        [OP_BUILD_SUBCLASS] = &&OP_BUILD_SUBCLASS,
        [OP_GET_ATTR] = &&OP_GET_ATTR,
        [OP_SET_ATTR] = &&OP_SET_ATTR,
        [OP_THIS] = &&OP_THIS,
        [OP_SUPER] = &&OP_SUPER,
        [OP_GET_ITEM] = &&OP_GET_ITEM,
        [OP_SET_ITEM] = &&OP_SET_ITEM,
//        [OP_DEL_ITEM] = &&OP_DEL_ITEM,
        [OP_BUILD_TUPLE] = &&OP_BUILD_TUPLE,
        [OP_BUILD_STRING] = &&OP_BUILD_STRING,
        [OP_FORMAT] = &&OP_FORMAT,
        [OP_BUILD_TABLE] = &&OP_BUILD_TABLE,
        [OP_ENTER_BLOCK] = &&OP_ENTER_BLOCK,
        [OP_LEAVE_BLOCK] = &&OP_LEAVE_BLOCK,
        [OP_BREAK] = &&OP_BREAK,
        [OP_CONTINUE] = &&OP_CONTINUE,
        [OP_GET_ITERATOR] = &&OP_GET_ITERATOR,
        [OP_NEXT_OR_BREAK] = &&OP_NEXT_OR_BREAK,
        [OP_ASSERT] = &&OP_ASSERT,
    };

#define DISPATCH() goto *_labels[(++pc)->op]

    // DISPATCH()
    goto *_labels[pc->op];

    for (;;) {
OP_JUMP:
            pc += pc->arg;
            DISPATCH();

OP_POP_JUMP_IF_TRUE:
            lhs = POP(stack);
            if (Bool_isTrue(lhs))
                pc += pc->arg;
            DECREF(lhs);
            DISPATCH();

OP_JUMP_IF_TRUE:
            lhs = PEEK(stack);
            if (Bool_isTrue(lhs))
                pc += pc->arg;
            DISPATCH();

OP_JUMP_IF_TRUE_OR_POP:
            lhs = PEEK(stack);
            if (Bool_isTrue(lhs))
                pc += pc->arg;
            else
                XPOP(stack);
            DISPATCH();

OP_POP_JUMP_IF_FALSE:
            lhs = POP(stack);
            if (!Bool_isTrue(lhs))
                pc += pc->arg;
            DECREF(lhs);
            DISPATCH();

OP_JUMP_IF_FALSE:
            lhs = PEEK(stack);
            if (!Bool_isTrue(lhs))
                pc += pc->arg;
            DISPATCH();

OP_JUMP_IF_FALSE_OR_POP:
            lhs = PEEK(stack);
            if (!Bool_isTrue(lhs))
                pc += pc->arg;
            else
                XPOP(stack);
            DISPATCH();

OP_DUP_TOP:
            lhs = PEEK(stack);
            PUSH(stack, lhs);
            DISPATCH();

OP_POP_TOP:
            POP(stack);
            DISPATCH();

OP_ASSERT:
            if ((pc->arg & ASSERT_FLAG_FAILED) == 0)
                // Need to test the top-of-stack for truthy
                if (Bool_isTrue(POP(stack)))
                    DISPATCH();

            // This is FATAL if we arrive here
            if ((pc->arg & ASSERT_FLAG_HAS_MESSAGE) != 0) {
                item = POP(stack);
                vmeval_raise(ctx, Exception_fromObject(item));
                DECREF(item);
            }
            else {
                    vmeval_raise(ctx, Exception_fromConstant("Assertion failed"));
            }
            // XXX: Will never make it here
            DISPATCH();

OP_CLOSE_FUN: {
            LoxVmCode *code = (LoxVmCode*) POP(stack);
            assert_safe_code(code->context);
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
            DISPATCH();
        }

OP_CALL_FUN: {
            Object *fun = *(stack - pc->arg - 1);

            if (VmFunction_isVmFunction(fun)) {
                VmEvalContext call_ctx = (VmEvalContext) {
                    .code = ((LoxVmFunction*)fun)->code->context,
                    .scope = ((LoxVmFunction*)fun)->scope,
                    .previous = ctx,
                    .args = (VmCallArgs) {
                        .values = stack - pc->arg,
                        .count = pc->arg,
                    },
                };
                rv = LoxVM_eval(&call_ctx);
            }
            else if (Function_isCallable(fun)) {
                LoxTuple *args = Tuple_fromList(pc->arg, stack - pc->arg);
                rv = fun->type->call(fun, ctx->scope, ctx->this, (Object*) args);
                INCREF(rv);
                if (Exception_isException(rv)) {
                    // exceptions from VM code will happen in the block above
                    vmeval_raise_exception(ctx, rv);
                }
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
            DISPATCH();
        }

OP_RECURSE: {
            // This will only happen for a LoxVmFunction. In this case, we will
            // execute the same code again, but with different arguments.
            VmEvalContext call_ctx = *ctx;
            call_ctx.previous = ctx;
            call_ctx.args = (VmCallArgs) {
                .values = stack - pc->arg,
                .count = pc->arg,
            };
            // Don't INCREF because the return value of the call should be
            // decref'd in this scope which would cancel an INCREF
            rv = LoxVM_eval(&call_ctx);

            // DECREF all the arguments
            i = pc->arg;
            while (i--)
                XPOP(stack);

            XPUSH(stack, rv);
            DISPATCH();
        }

OP_BUILD_SUBCLASS:
            item = POP(stack);               // (parent)
            // Build the class as usual
OP_BUILD_CLASS: {
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
                pc->op == OP_BUILD_SUBCLASS ? (LoxClass*) item : NULL));
            if (pc->op == OP_BUILD_SUBCLASS)
                DECREF(item);
            DISPATCH();
        }

OP_GET_ATTR:
            C = ctx->code->constants + pc->arg;
            lhs = POP(stack);
            PUSH(stack, object_getattr(lhs, C->value, C->hash));
            DECREF(lhs);
            DISPATCH();

OP_SET_ATTR:
            C = ctx->code->constants + pc->arg;
            rhs = POP(stack);
            lhs = POP(stack);
            if (unlikely(!lhs->type->setattr)) {
                fprintf(stderr, "WARNING: `setattr` not defined for type: `%s`\n", lhs->type->name);
            }
            else {
                lhs->type->setattr(lhs, C->value, rhs, C->hash);
            }
            DECREF(rhs);
            DECREF(lhs);
            DISPATCH();

OP_THIS:
            PUSH(stack, ctx->this);
            DISPATCH();

OP_SUPER: {
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
            DISPATCH();
        }

OP_LOOKUP_GLOBAL:
            C = ctx->code->constants + pc->arg;
            item = VmScope_lookup_global(ctx->scope, C->value, C->hash);
            if (LoxNativeProperty_isProperty(item))
                item = LoxNativeProperty_callGetter(item, ctx->scope, ctx->this);
            PUSH(stack, item);
            DISPATCH();

OP_LOOKUP_CLOSED:
            if (ctx->scope) {
                lhs = VmScope_lookup_local(ctx->scope, pc->arg);
            }
            else
                lhs = LoxUndefined;
            PUSH(stack, lhs);
            DISPATCH();

OP_STORE_GLOBAL:
            C = ctx->code->constants + pc->arg;
            lhs = POP(stack);
            VmScope_assign(ctx->scope, C->value, lhs, C->hash);
            DECREF(lhs);
            DISPATCH();

OP_STORE_LOCAL:
            item = *(locals + pc->arg);
            DECREF(item);
            *(locals + pc->arg) = POP(stack);
            DISPATCH();

OP_LOOKUP_LOCAL:
            PUSH(stack, *(locals + pc->arg));
            DISPATCH();

OP_CONSTANT:
            C = ctx->code->constants + pc->arg;
            PUSH(stack, C->value);
            DISPATCH();

#define COMPARE(lhs, rhs) \
    lhs == rhs ? 0 : \
        (lhs->type->compare ? lhs->type->compare(lhs, rhs) : \
             (rhs->type->compare ? - rhs->type->compare(rhs, lhs) : \
                 -1))

#define BINARY_COMPARE() ({ \
    rhs = POP(stack); \
    lhs = POP(stack); \
    int x = COMPARE(lhs, rhs); \
    DECREF(lhs); \
    DECREF(rhs); \
    x; \
})

        // Comparison
OP_COMPARE:
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
                if (lhs->type != rhs->type)
                    PUSH(stack, (Object*) LoxFALSE);
                else if (COMPARE(lhs, rhs) == 0)
                    PUSH(stack, (Object*) LoxTRUE);
                else
                    PUSH(stack, (Object*) LoxFALSE);
                DECREF(lhs);
                DECREF(rhs);
                break;

            case COMPARE_NOT_EXACT:
                rhs = POP(stack);
                lhs = POP(stack);
                if (lhs->type != rhs->type)
                    PUSH(stack, (Object*) LoxTRUE);
                else if (COMPARE(lhs, rhs) != 0)
                    PUSH(stack, (Object*) LoxFALSE);
                else
                    PUSH(stack, (Object*) LoxTRUE);
                DECREF(lhs);
                DECREF(rhs);
                break;

            case COMPARE_IS:
                rhs = POP(stack);
                lhs = POP(stack);
                PUSH(stack, (Object*) (lhs == rhs ? LoxTRUE : LoxFALSE));
                DECREF(lhs);
                DECREF(rhs);
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
            case COMPARE_SPACESHIP:
                PUSH(stack, (Object*) Integer_fromLongLong(BINARY_COMPARE()));
                break;
            }
            DISPATCH();

        // Boolean
OP_BANG:
            lhs = POP(stack);
            PUSH(stack, (Object*) (Bool_isTrue(lhs) ? LoxFALSE : LoxTRUE));
            DECREF(lhs);
            DISPATCH();

        // Expressions
OP_BINARY_MATH:
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
                fprintf(stderr, "WARNING: Type `%s` does not support op `%hd`\n", lhs->type->name, pc->arg);
                PUSH(stack, LoxUndefined);
            }
            DECREF(lhs);
            DECREF(rhs);
            DISPATCH();

OP_UNARY_NEGATIVE:
            lhs = POP(stack);
            PUSH(stack, (Object*) lhs->type->op_neg(lhs));
            DECREF(lhs);
            DISPATCH();

OP_UNARY_INVERT:
            DISPATCH();

OP_GET_ITEM:
            rhs = POP(stack);
            lhs = POP(stack);
            if (lhs->type->get_item)
                PUSH(stack, lhs->type->get_item(lhs, rhs));
            else
                fprintf(stderr, "lhs type `%s` does not support GET_ITEM\n", lhs->type->name);
            DECREF(lhs);
            DECREF(rhs);
            DISPATCH();

OP_SET_ITEM:
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
            DISPATCH();

OP_BUILD_TUPLE:
            item = (Object*) Tuple_fromList(pc->arg, stack - pc->arg);
            i = pc->arg;
            while (i--)
                XPOP(stack);
            PUSH(stack, item);
            DISPATCH();

OP_BUILD_STRING:
            item = LoxString_BuildFromList(pc->arg, stack - pc->arg);
            i = pc->arg;
            while (i--)
                XPOP(stack);
            PUSH(stack, item);
            DISPATCH();

OP_FORMAT:
            C = ctx->code->constants + pc->arg;
            assert(String_isString(C->value));
            item = LoxObject_Format(POP(stack), ((LoxString*) C->value)->characters);
            PUSH(stack, item);
            DISPATCH();

OP_BUILD_TABLE:
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
            DISPATCH();

OP_ENTER_BLOCK:
            *(++pblock) = (VmEvalLoopBlock) {
                .top = pc,
                .bottom = pc + pc->arg,
            };
            DISPATCH();

OP_CONTINUE:
            pc = pblock->top;
            DISPATCH();

OP_LEAVE_BLOCK:
            pblock--;
            i = pc->arg;
            while (i--)
                XPOP(stack);
            DISPATCH();

OP_NEXT_OR_BREAK:
            lhs = PEEK(stack);
            item = ((Iterator*) lhs)->next((Iterator*) lhs);
            if (item != LoxStopIteration && item != NULL) {
                Object *old = *(locals + pc->arg);
                if (old)
                    DECREF(old);
                *(locals + pc->arg) = item;
                INCREF(item);
            }
            else {
OP_BREAK:
                pc = pblock->bottom;
            }
            DISPATCH();

OP_GET_ITERATOR:
            lhs = POP(stack);
            if (lhs->type->iterate) {
                item = (Object*) lhs->type->iterate(lhs);
            }
            else {
                fprintf(stderr, "Type `%s` is not iterable\n", lhs->type->name);
                item = LoxUndefined;
            }
            PUSH(stack, item);
            DECREF(lhs);
            DISPATCH();

OP_NOOP:
            DISPATCH();
OP_HALT:
OP_RETURN:
            break;
    }

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

    return rv;
}

static Object*
LoxVM_evalWithScope(CodeContext *code, VmScope *scope) {
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
        .previous = NULL,
    };
    return LoxVM_evalSafely(&ctx);
}

Object*
LoxVM_evalStringWithScope(const char * text, size_t length, VmScope* scope) {
    Compiler compiler = { .flags = 0 };
    CodeContext *context;

    context = compile_string(&compiler, text, length);

    return LoxVM_evalWithScope(context, scope);
}

Object*
LoxVM_evalString(const char * text, size_t length) {
    return LoxVM_evalStringWithScope(text, length, NULL);
}

Object*
LoxVM_evalFileWithScope(FILE *input, const char* filename, VmScope* scope) {
    Compiler compiler = { .flags = 0 };
    CodeContext *context;

    context = compile_file(&compiler, input, filename);

    return LoxVM_evalWithScope(context, scope);
}

Object*
LoxVM_evalFile(FILE *input, const char* filename) {
    return LoxVM_evalFileWithScope(input, filename, NULL);
}

Object*
LoxVM_evalAST(ASTNode *input) {
    Compiler compiler = { .flags = 0 };
    CodeContext *context;

    context = compile_ast(&compiler, input);
    return LoxVM_evalWithScope(context, NULL);
}
