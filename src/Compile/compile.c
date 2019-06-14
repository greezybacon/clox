#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

#include "vm.h"
#include "compile.h"
#include "Parse/debug_parse.h"
#include "Objects/function.h"
#include "Vendor/bdwgc/include/gc.h"

static CompileResult compile_node_count(Compiler *self, ASTNode* ast, unsigned *count);
static CompileResult compile_node(Compiler *self, ASTNode* ast);
static CompileResult compile_node1(Compiler *self, ASTNode* ast, CompileResult*);

#define max(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

static void
compile_error(Compiler* self, char* format, ...) {
    va_list args;

    va_start(args, format);
    fprintf(stderr, "Warning: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// Function compilation
static CodeContext*
compile_pop_context(Compiler* self) {
    // Set the context of the compiler to be the previous, return the current
    CodeContext *rv = self->context;
    self->context = self->context->prev;

#ifdef DEBUG
    print_codeblock(rv, rv->block);
    printf("--------\n");
#endif

    return rv;
}

static void
compile_start_block(Compiler *self) {
    CodeBlock *block = GC_MALLOC(sizeof(CodeBlock));
    *block = (CodeBlock) {
        .size = 32 * sizeof(Instruction),
        .prev = self->context->block,
        .instructions = GC_MALLOC(32 * sizeof(Instruction)),
    };
    self->context->block = block;
}

static void
compile_push_context(Compiler* self) {
    CodeContext *context = GC_MALLOC(sizeof(CodeContext));
    *context = (CodeContext) {
        .constants = GC_MALLOC(8 * sizeof(Constant)),
        .sizeConstants = 8,
        .locals = (LocalsList) {
            .size = 8,
            .vars = GC_MALLOC(8 * sizeof(LocalVariable)),
        },
        .prev = self->context,
    };
    self->context = context;
    compile_start_block(self);
}

#define SetBit(A,k)     ( A[(k/32)] |= (1 << (k%32)) )
#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) )
#define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) )

static int
compile_reserve_register(Compiler *self) {
    RegisterStatus *r = &self->registers;

    int out = r->lowest_avail;
    while (out < 257) {
        if (!TestBit(r->used, out))
            break;
        out++;
    }

    if (unlikely(out == 256)) {
        fprintf(stderr, "Unable to allocate a register\n");
        return -1;
    }

    r->high_water_mark = max(r->high_water_mark, out);

    SetBit(r->used, out);
    return out;
}

static void
compile_register_setused(Compiler *self, int number) {
    RegisterStatus *r = &self->registers;

    assert(!TestBit(r->used, number));
    SetBit(r->used, number);
}

static void
compile_release_register(Compiler *self, int number) {
    RegisterStatus *r = &self->registers;

    assert(TestBit(r->used, number));
    ClearBit(r->used, number);

    r->lowest_avail = min(r->lowest_avail, number);
}

static BlockCompileResult
compile_block(Compiler *self, ASTNode* node) {
    compile_start_block(self);

    // Nest the compiler to auto-release registers and mark the output of the
    // block as in-use if it's a register
    Compiler nested = *self;
    CompileResult result = compile_node(&nested, node);
    if (result.location == OP_VAR_LOCATE_REGISTER)
        compile_register_setused(self, result.index);

    // Pop the started block
    CodeBlock *block = self->context->block;
    self->context->block = block->prev;
    return (BlockCompileResult) {
        .block = block,
        .result = result,
    };
}

static inline void
compile_block_ensure_size(CodeBlock *block, unsigned size) {
    if (size > block->size) {
        while (block->size < size) {
            block->size += 16;
        }
        block->instructions = GC_REALLOC(block->instructions,
            block->size * sizeof(Instruction));
    }
}

static void
compile_merge_block(Compiler *self, CodeBlock* block) {
    CodeBlock *current = self->context->block;

    compile_block_ensure_size(current, current->bytes + block->bytes);
    memcpy(current->instructions + current->bytes, block->instructions, block->bytes);
    current->bytes += block->bytes;
}

static unsigned short
compile_emit_constant(Compiler *self, Object *value) {
    CodeContext *context = self->context;
    unsigned short index = 0;

    // Short-circuit this process for emitting repeat constants
    Constant *C = context->constants;
    while (index < context->nConstants) {
        if (C->value->type == value->type
                && 0 == value->type->compare(value, C->value))
            return index;
        index++;
        C++;
    }

    if (context->nConstants == context->sizeConstants) {
        unsigned new_size = context->sizeConstants + 8;
        C = GC_REALLOC(context->constants, new_size * sizeof(Constant));
        if (!C)
            // TODO: Raise compiler error?
            return 0;
        context->constants = C;
    }

    index = context->nConstants++;
    *(context->constants + index) = (Constant) {
        .value = value,
        .hash = (value->type->hash) ? value->type->hash(value) : 0,
    };
    INCREF(value);
    return index;
}

static void*
compile_emit3(Compiler *self, const ShortInstruction opcode, unsigned length) {
    CodeBlock *block = self->context->block;

    compile_block_ensure_size(block, block->bytes + length);
    memcpy(block->instructions + block->bytes, &opcode, length);

    void *result = block->instructions + block->bytes;
    block->bytes += length;

    return result;
}

static void*
compile_emit2(Compiler *self, const Instruction opcode, unsigned length) {
    CodeBlock *block = self->context->block;

    compile_block_ensure_size(block, block->bytes + length);
    memcpy(block->instructions + block->bytes, &opcode, length);

    void *result = block->instructions + block->bytes;
    block->bytes += length;

    return result;
}

static void
compile_emit_args(Compiler *self, ASTNode *node, unsigned count) {
    CodeBlock *block = self->context->block;
    CompileResult result;
    ShortArg arg;

    while (count-- && node) {
        result = compile_node1(self, node, NULL);
        arg = (ShortArg) {
            .location = result.location,
            .index = result.index
        };
        memcpy(block->instructions + block->bytes, &arg, sizeof(ShortArg));
        block->bytes += sizeof(ShortArg);
        node = node->next;
    }
}

static int
compile_locals_islocal(CodeContext *context, Object *name, hashval_t hash) {
    assert(name->type->compare);

    int index = 0;
    LocalsList *locals = &context->locals;

    // Direct search through the locals list
    while (index < locals->count) {
        if ((locals->vars + index)->name.hash == hash
            && 0 == name->type->compare(name, (locals->vars + index)->name.value)
        ) {
            // Already in the locals list
            return index;
        }
        index++;
    }

    return -1;
}

/**
 * Returns 0-based local slot of local variable in the immediately-outer closed
 * context. Returns -1 if the context is not nested or the variable is not
 * closed in the outer context.
 */
static int
compile_locals_isclosed(Compiler *self, Object *name) {
    if (!self->context->prev)
        return -1;

    hashval_t hash = HASHVAL(name);
    return compile_locals_islocal(self->context->prev, name, hash);

    // TODO: Recurse through further nesting?
}

static unsigned
compile_locals_allocate(Compiler *self, Object *name) {
    assert(name->type);

    int index;
    LocalsList *locals = &self->context->locals;

    hashval_t hash = HASHVAL(name);
    if (-1 != (index = compile_locals_islocal(self->context, name, hash))) {
        return index;
    }

    // Ensure space in the index and locals
    if (locals->size <= locals->count) {
        locals->size += 8;
        locals->vars = GC_REALLOC(locals->vars, sizeof(*locals->vars) * locals->size);
    }

    index = locals->count++;
    *(locals->vars + index) = (LocalVariable) {
        .name = {
            .value = name,
            .hash = hash,
        },
        .regnum = compile_reserve_register(self),
    };
    return index;
}

static void
compile_push_info(Compiler *self, CompileInfo info) {
    CompileInfo *new = malloc(sizeof(CompileInfo));
    if (self->info) {
        *new = (CompileInfo) {
            .function_name = info.function_name ? info.function_name
                : self->info->function_name,
            .class_name = info.class_name ? info.class_name
                : self->info->class_name,
        };
    }
    else {
        *new = info;
    }
    new->prev = self->info;
    self->info = new;
}

static void
compile_pop_info(Compiler *self) {
    if (self->info && self->info->prev) {
        CompileInfo *old = self->info;
        self->info = self->info->prev;
        free(old);
    }
    else {
        free(self->info);
        self->info = NULL;
    }
}

static CompileResult
compile_assignment(Compiler *self, ASTAssignment *assign) {
    // Push the expression
    CompileResult expr = compile_node(self, assign->expression), result;
    unsigned index;

    if (self->flags & CFLAG_LOCAL_VARS) {
        // XXX: Consider assignment to non-local (closed) vars?
        // Lookup or allocate a local variable
        index = compile_locals_allocate(self, assign->name);
        result = (CompileResult) {
            .index = (self->context->locals.vars + index)->regnum,
            .location = OP_VAR_LOCATE_REGISTER,
        };
    }
    else {
        // Non-local
        // Find the name in the constant stack
        index = compile_emit_constant(self, assign->name);
        // Push the STORE
        result = (CompileResult) {
            .index = index,
            .location = OP_VAR_LOCATE_GLOBAL,
        };
    }

    if (expr.islookup) {
        compile_emit3(self, (ShortInstruction) {
            .opcode = ROP_STORE,
            .flags.lro.lhs = result.location,
            .flags.lro.rhs = expr.location,
            .p1 = result.index,
            .p2 = expr.index,
        }, ROP_STORE__LEN);
    }
    else {
        // TODO: The output of the expression compilation is in `expr`. The last instruction
        // should be in the current codeblock and should be rewindable based on the `bytes`
        // or `length` in the result.
        CodeBlock *block = self->context->block;
        Instruction *previous = ((void*) block->instructions) + block->bytes - expr.length;

        // Rewrite previous instruction to place output in the local variable.
        // TODO: Release reserved register
        // TODO: Consider type of previous instruction. MATH is assumed
        if (previous->opcode == ROP_CALL) {
            previous->p1 = result.index;
        }
        else {
            previous->p3 = result.index;
        }
        previous->flags.lro.out = result.location;
    }

    return result;
}

static struct token_to_math_op {
    enum token_type     token;
    enum opcode         vm_opcode;
    union {
        enum lox_vm_math    math_op;
        enum lox_vm_compare compare_op;
    };
    const char          *op;
} TokenToMathOp[] = {
    { T_OP_PLUS,        ROP_MATH,       MATH_BINARY_PLUS,       "+" },
    { T_OP_MINUS,       ROP_MATH,       MATH_BINARY_MINUS,      "-" },
    { T_OP_STAR,        ROP_MATH,       MATH_BINARY_STAR,       "*" },
    { T_OP_SLASH,       ROP_MATH,       MATH_BINARY_SLASH,      "/" },
    { T_OP_AMPERSAND,   ROP_MATH,       MATH_BINARY_AND,        "&" },
    { T_OP_PIPE,        ROP_MATH,       MATH_BINARY_OR,         "|" },
    { T_OP_CARET,       ROP_MATH,       MATH_BINARY_XOR,        "^" },
    { T_OP_PERCENT,     ROP_MATH,       MATH_BINARY_MODULUS,    "%" },
    { T_OP_LSHIFT,      ROP_MATH,       MATH_BINARY_LSHIFT,     "<<" },
    { T_OP_RSHIFT,      ROP_MATH,       MATH_BINARY_RSHIFT,     ">>" },

    { T_OP_EQUAL,       ROP_COMPARE,    COMPARE_EQ,             "==" },
    { T_OP_NOTEQUAL,    ROP_COMPARE,    COMPARE_NOT_EQ,         "!=" },
    { T_OP_LT,          ROP_COMPARE,    COMPARE_LT,             "<" },
    { T_OP_LTE,         ROP_COMPARE,    COMPARE_LTE,            "<=" },
    { T_OP_GT,          ROP_COMPARE,    COMPARE_GT,             ">" },
    { T_OP_GTE,         ROP_COMPARE,    COMPARE_GTE,            ">=" },
    { T_OP_IN,          ROP_COMPARE,    COMPARE_IN,             "in" },
};

static int _cmpfunc (const void * a, const void * b) {
   return ((struct token_to_math_op*) a)->token - ((struct token_to_math_op*) b)->token;
}

static CompileResult
compile_expression(Compiler* self, ASTExpression *expr, CompileResult *intermediate) {
    static bool lookup_sorted = false;
    if (!lookup_sorted) {
        qsort(TokenToMathOp, sizeof(TokenToMathOp) / sizeof(struct token_to_math_op),
            sizeof(struct token_to_math_op), _cmpfunc);
        lookup_sorted = true;
    }

    // Push the LHS
    BlockCompileResult lhs = compile_block(self, expr->lhs);
    compile_merge_block(self, lhs.block);

    CompileResult result = { 0 };
    Instruction *ins = NULL;

    // Perform the binary op
    if (expr->binary_op) {
        // Compile the RHS in a sub-block so we can get a length to jump
        // over and emit the code after the short-circuit logic. This will auto-free
        // registers reserved in the nested context.
        BlockCompileResult rhs = compile_block(self, expr->rhs);

        // Handle short-circuit logic first
        switch (expr->binary_op) {
        case T_AND:
        case T_OR:
            // If the LHS is false, then jump past the end of the expression 
            // as we've already decided on the answer
            compile_emit2(self, (Instruction) {
                .opcode = ROP_CONTROL,
                .subtype = expr->binary_op == T_AND
                    ? OP_CONTROL_JUMP_IF_FALSE : OP_CONTROL_JUMP_IF_TRUE,
                .flags.lro.lhs = lhs.result.location,
                .p1 = lhs.result.index,
                .p23 = rhs.block->bytes,
            }, ROP_CONTROL__LEN);
            break;

        default:
            break;
        }

        // Emit the RHS
        compile_merge_block(self, rhs.block);

        switch (expr->binary_op) {
        case T_OP_PLUS:
        case T_OP_MINUS:
        case T_OP_STAR:
        case T_OP_SLASH:
        case T_OP_CARET:
        case T_OP_PERCENT:
        case T_OP_LSHIFT:
        case T_OP_RSHIFT:
        case T_OP_AMPERSAND:
        case T_OP_PIPE:
        case T_OP_TILDE: {
            int out = compile_reserve_register(self);
            struct token_to_math_op* T, key = { .token = expr->binary_op };
            T = bsearch(&key, TokenToMathOp, sizeof(TokenToMathOp) / sizeof(struct token_to_math_op),
                sizeof(struct token_to_math_op), _cmpfunc);

            // TODO: Consider utility function like compile_emit_MATH
            ins = compile_emit2(self, (Instruction) {
                .opcode     = ROP_MATH,
                .subtype    = T->math_op,
                .flags.lro.out = OP_VAR_LOCATE_REGISTER,
                .flags.lro.rhs = rhs.result.location,
                .flags.lro.lhs = lhs.result.location,
                .p1         = lhs.result.index,
                .p2         = rhs.result.index,
                .p3         = out,
            }, ROP_MATH__LEN);
            
            result = (CompileResult) {
                .location = OP_VAR_LOCATE_REGISTER,
                .index = out,
            };
            break;
        }

        case T_OP_EQUAL:
        case T_OP_NOTEQUAL:
        case T_OP_LT:
        case T_OP_LTE:
        case T_OP_GT:
        case T_OP_GTE:
        case T_OP_IN: {
            int out = compile_reserve_register(self);
            struct token_to_math_op* T, key = { .token = expr->binary_op };
            T = bsearch(&key, TokenToMathOp, sizeof(TokenToMathOp) / sizeof(struct token_to_math_op),
                sizeof(struct token_to_math_op), _cmpfunc);

            // TODO: Consider utility function like compile_emit_MATH
            ins = compile_emit2(self, (Instruction) {
                .opcode     = ROP_COMPARE,
                .subtype    = T->math_op,
                .flags.lro.out = OP_VAR_LOCATE_REGISTER,
                .flags.lro.rhs = rhs.result.location,
                .flags.lro.lhs = lhs.result.location,
                .p1         = lhs.result.index,
                .p2         = rhs.result.index,
                .p3         = out,
            }, ROP_COMPARE__LEN);
        
            result = (CompileResult) {
                .location = OP_VAR_LOCATE_REGISTER,
                .index = out,
            };
            break;
        }

        case T_AND:
        case T_OR:
            // already handled above
            break;

        default:
            compile_error(self, "Parser error: Unexpected binary operator: %d", expr->binary_op);
        }
    }

    // Perform the unary op. Note that if binary and unary operations are
    // combined, then it is assumed to have been compiled from something like
    // -(a * b)
    switch (expr->unary_op) {
    // TODO: Bitwise NOT?
    // TODO: Implement COPY math operation where out = lhs to support flipping
    // loading only LHS and flipping the output
    case T_BANG:
        if (ins)
            ins->flags.lro.opt2 = 1;
        break;

    case T_OP_MINUS:
        if (ins)
            ins->flags.lro.opt1 = 1;
        break;

    case T_OP_PLUS:
    default:
        // This is really a noop
        break;
    }

    return result;
}

/*
static unsigned
compile_tuple_literal(Compiler *self, ASTTupleLiteral *node) {
    unsigned length = 0, count = 0;
    ASTNode* item = node->items;

    compile_reserve_contiguous_registers(self, node->count);
    while (item) {
        length += compile_node1(self, item, NULL);
        count++;
        item = item->next;
    }

    return length + compile_emit(self, OP_BUILD_TUPLE, count);
}
*/

static CompileResult
compile_return(Compiler *self, ASTReturn *node) {
    if (node->expression) {
        CompileResult result = compile_node(self, node->expression);
        compile_emit2(self, (Instruction) {
            .opcode = ROP_CONTROL,
            .subtype = OP_CONTROL_RETURN,
            .flags.lro.out = result.location,
            .p1 = result.index,
        }, ROP_CONTROL__LEN);
        return result;
    }
    else {
        unsigned index = compile_emit_constant(self, LoxNIL);
        compile_emit2(self, (Instruction) {
            .opcode = ROP_CONTROL,
            .subtype = OP_CONTROL_RETURN,
            .flags.lro.out = OP_VAR_LOCATE_CONSTANT,
            .p1 = index,
        }, ROP_CONTROL__LEN);
        return (CompileResult) {
            .index = index,
        };
    }
}

static CompileResult
compile_literal(Compiler *self, ASTLiteral *node) {
    // Fetch the index of the constant
    unsigned index = compile_emit_constant(self, node->literal);
    return (CompileResult) {
        .islookup = 1,
        .index = index,
        .location = OP_VAR_LOCATE_CONSTANT,
    };
}

/*
static unsigned
compile_interpolated_string(Compiler *self, ASTInterpolatedString *node) {
    unsigned length = 0, count;
    length += compile_node_count(self, node->items, &count);
    return length + compile_emit(self, OP_BUILD_STRING, count);
}

static unsigned
compile_interpolated_expr(Compiler *self, ASTInterpolatedExpr *node) {
    unsigned length;
    length = compile_node(self, node->expr);

    if (node->format) {
        LoxString *format = String_fromLiteral(node->format, strlen(node->format));
        unsigned index = compile_emit_constant(self, (Object*) format);
        length += compile_emit(self, OP_FORMAT, index);
    }

    return length;
}
*/

static CompileResult
compile_lookup(Compiler *self, ASTLookup *node) {
    // See if the name is in the locals list
    int index;
    if (-1 != (index = compile_locals_islocal(self->context, node->name, HASHVAL(node->name)))) {
        return (CompileResult) {
            .islookup = 1,
            .location = OP_VAR_LOCATE_REGISTER,
            .index = index,
        };
    }
    else if (-1 != (index = compile_locals_isclosed(self, node->name))) {
        return (CompileResult) {
            .islookup = 1,
            .location = OP_VAR_LOCATE_CLOSURE,
            .index = index,
        };
    }
    // Fetch the index of the name constant
    return (CompileResult) {
        .islookup = 1,
        .location = OP_VAR_LOCATE_GLOBAL,
        .index = compile_emit_constant(self, node->name),
    };
}

static CompileResult
compile_magic(Compiler *self, ASTMagic *node) {
    return (CompileResult) {
        .location = OP_VAR_LOCATE_CONSTANT,
        .index = (node->this) ? OP_SPECIAL_CONSTANT_THIS :OP_SPECIAL_CONSTANT_SUPER,
        .islookup = 1,
    };
}

static CompileResult
compile_while(Compiler* self, ASTWhile *node) {
    unsigned start = self->context->block->bytes;

    // Emit the condition
    // Emit the block in a different context, capture the length
    BlockCompileResult block = compile_block(self, node->block);
    CompileResult condition = compile_node(self, node->condition);

    // Jump over the block
    compile_emit2(self, (Instruction) {
        .opcode = ROP_CONTROL,
        .subtype = OP_CONTROL_JUMP_IF_FALSE,
        .flags.lro.out = condition.location,
        .p1 = condition.index,
        .p23 = block.block->bytes,
    }, ROP_CONTROL__LEN);

    // Do the block
    compile_merge_block(self, block.block);

    // Jump back and re-eval loop condition
    // TODO: Could be OP_CONTROL_LOOP_CONTINUE
    compile_emit2(self, (Instruction) {
        .opcode = ROP_CONTROL,
        .subtype = OP_CONTROL_JUMP,
        .p23 = -condition.length - block.block->bytes - ROP_CONTROL__LEN * 2,
    }, ROP_CONTROL__LEN);

    return (CompileResult) { 0 };
}

static CompileResult
compile_if(Compiler *self, ASTIf *node) {
    // Emit the condition
    CompileResult condition = compile_node(self, node->condition);

    // Compile (but not emit) the code block
    BlockCompileResult block = compile_block(self, node->block);

    // Jump over the block if condition is false
    // TODO: The JUMP could perhaps be omitted (if the end of `block` is RETURN)
    // ... 1 for JUMP below (if node->otherwise)
    compile_emit2(self, (Instruction) {
        .opcode = ROP_CONTROL,
        .subtype = OP_CONTROL_JUMP_IF_FALSE,
        .flags.lro.out = condition.location,
        .p1 = condition.index,
        .p23 = block.block->bytes + (node->otherwise ? ROP_CONTROL__LEN : 0),
    }, ROP_CONTROL__LEN);

    // Add in the block following the condition
    compile_merge_block(self, block.block);

    // If there's an otherwise, then emit it
    if (node->otherwise) {
        block = compile_block(self, node->otherwise);
        // (As part of the positive/previous block), skip the ELSE part.
        // TODO: If the last line was RETURN, this could be ignored
        compile_emit2(self, (Instruction) {
            .opcode = ROP_CONTROL,
            .subtype = OP_CONTROL_JUMP,
            .p23 = block.block->bytes,
        }, ROP_CONTROL__LEN);

        compile_merge_block(self, block.block);
    }
    return (CompileResult) { 0 };
}

static CompileResult
compile_function_inner(Compiler *self, ASTFunction *node) {
    // Create a new compiler context for the function's code (with new constants)
    compile_push_context(self);

    ASTNode *p;
    ASTFuncParam *param;
    unsigned index, length = 0;

    // (When the function is executed), load the arguments off the args list
    // Into local variables. The arguments will be in the proper order already,
    // so no need to reverse. The unpacking is actually done by the interpreter,
    // but here we need to allocate the local variables for the parameters.
    for (p = node->arglist; p != NULL; p = p->next) {
        assert(p->type == AST_PARAM);
        param = (ASTFuncParam*) p;
        compile_locals_allocate(self,
            (Object*) String_fromCharArrayAndSize(param->name, param->name_length));
    }

    // Compile the function's code in the new context
    // Local vars are welcome inside the function
    Compiler nested = *self;
    nested.flags |= CFLAG_LOCAL_VARS;
    compile_node(&nested, node->block);

    // Create a constant for the function
    index = compile_emit_constant(self,
        VmCode_fromContext(node, compile_pop_context(self)));

    // (Meanwhile, back in the original context)
    return (CompileResult) {
        .index = index,
        .location = OP_VAR_LOCATE_CONSTANT,
    };
}

static CompileResult
compile_function(Compiler *self, ASTFunction *node) {
    Object *name = NULL;

    if (node->name_length) {
        name = (Object*) String_fromCharArrayAndSize(node->name, node->name_length);
        compile_push_info(self, (CompileInfo) {
            .function_name = name,
        });
    }

    CompileResult func = compile_function_inner(self, node);
    unsigned location, index;

    // Store the named function?
    if (name) {
        if (self->flags & CFLAG_LOCAL_VARS) {
            index = compile_locals_allocate(self, name);
            location = OP_VAR_LOCATE_REGISTER;
        }
        else {
            index = compile_emit_constant(self, name);
            location = OP_VAR_LOCATE_GLOBAL;
        }
        compile_pop_info(self);
    }
    else {
        index = compile_reserve_register(self);
        location = OP_VAR_LOCATE_REGISTER;
    }

    // Create closure and stash in context
    compile_emit2(self, (Instruction) {
        .opcode = ROP_BUILD,
        .subtype = OP_BUILD_FUNCTION,
        .flags.lro.out = location,
        .flags.lro.rhs = func.location,
        .p1 = index,
        .p2 = func.index,
    }, ROP_BUILD__LEN_BASE);

    return (CompileResult) {
        .location = location,
        .index = index
    };
}

static CompileResult
compile_invoke(Compiler* self, ASTInvoke *node) {
    // Push the function / callable / lookup
    bool recursing = false;

    // TODO: If we are currently executing a function with the same name
    // as the one being invoked here, and the name of the function is not
    // local, then we use a RECURSE opcode which could be a special case
    // of CALL_FUN which adds a stack frame but does not lookup nor change
    // the code execution context.
    if (self->info && self->info->function_name
        && node->callable->type == AST_LOOKUP
    ) {
        Object *name = ((ASTLookup*) node->callable)->name;
        if (name->type == self->info->function_name->type
            && 0 == name->type->compare(name, self->info->function_name)
            && -1 == compile_locals_islocal(self->context, name, HASHVAL(name))
        ) {
            recursing = true;
        }
    }

    // Make the function
    CompileResult callable;
    if (!recursing)
        callable = compile_node(self, node->callable);

    unsigned out = compile_reserve_register(self);
    compile_emit2(self, (Instruction) {
        .opcode = ROP_CALL,
        .subtype = recursing ? 0 : callable.index,
        .flags.lro.rhs = callable.location,
        .flags.lro.out = OP_VAR_LOCATE_REGISTER,
        .flags.lro.opt1 = node->return_value_ignored,
        .flags.lro.opt2 = recursing,
        .p1 = out,
        .len = node->nargs,
    }, ROP_CALL__LEN_BASE);

    // Add all the arguments
    compile_emit_args(self, node->args, node->nargs);

    return (CompileResult) {
        .index = out,
        .location = OP_VAR_LOCATE_REGISTER,
    };
}

static CompileResult
compile_var(Compiler *self, ASTVar *node) {
    unsigned length = 0, index;

    // Make room in the locals for the name
    index = compile_locals_allocate(self, node->name);

    if (node->expression) {
        return compile_assignment(self, node);
    }

    return (CompileResult) {
        .index = index,
        .location = OP_VAR_LOCATE_REGISTER,
    };
}

/*
static CompileResult
compile_class(Compiler *self, ASTClass *node) {
    // PLAN: Build the class, build hashtable of methods, stash by name
    size_t length = 0, count = 0;
    unsigned index;

    // Build the methods
    ASTNode *body = node->body;
    ASTFunction *method;
    Object *method_name;
    while (body) {
        assert(body->type == AST_FUNCTION);
        method = (ASTFunction*) body;
        if (method->name_length) {
            method_name = (Object*) String_fromCharArrayAndSize(method->name,
                method->name_length);
            index = compile_emit_constant(self, method_name);
            compile_push_info(self, (CompileInfo) {
                .function_name = method_name,
            });
        }

        compile_function_inner(self, method);

        // Anonymous methods?
        if (method->name_length) {
            length += compile_emit(self, OP_CONSTANT, index);
            count++;
            compile_pop_info(self);
        }

        body = body->next;
    }

    // Inheritance
    if (node->extends) {
        length += compile_node(self, node->extends);
        length += compile_emit(self, OP_BUILD_SUBCLASS, count);
    }
    else {
        length += compile_emit(self, OP_BUILD_CLASS, count);
    }

    // Support anonymous classes?
    if (node->name) {
        if (self->flags & CFLAG_LOCAL_VARS) {
            index = compile_locals_allocate(self, node->name);
            length += compile_emit(self, OP_STORE_LOCAL, index);
        }
        else {
            index = compile_emit_constant(self, node->name);
            length += compile_emit(self, OP_STORE_GLOBAL, index);
        }
    }

    return (CompileResult) {
        .length = length,
    };
}
*/

static CompileResult
compile_attribute(Compiler *self, ASTAttribute *attr) {
    CompileResult value, object = compile_node(self, attr->object);

    unsigned index = compile_emit_constant(self, attr->attribute);
    if (attr->value) {
        value = compile_node(self, attr->value);
    }

    Instruction *ins = compile_emit2(self, (Instruction) {
        .opcode = ROP_ATTR,
        .subtype = attr->value ? OP_ATTRIBUTE_SET : OP_ATTRIBUTE_GET,
        .flags.lro.lhs = object.location,
        .flags.lro.rhs = OP_VAR_LOCATE_CONSTANT,
        .flags.lro.out = attr->value ? value.location : OP_VAR_LOCATE_REGISTER,
        .p1 = object.index,
        .p2 = compile_emit_constant(self, attr->attribute),
        .p3 = (attr->value) ? value.index : compile_reserve_register(self),
    }, ROP_ATTR__LEN);

    return (CompileResult) {
        .index = ins->p3,
        .location = ins->flags.lro.out,
    };
}

/*
static CompileResult
compile_slice(Compiler *self, ASTSlice *node) {
    unsigned length = compile_node(self, node->object),
             index;

    if (!node->end && !node->step) {
        length += compile_node(self, node->start);
    }
    if (node->value) {
        length += compile_node(self, node->value);
        return (CompileResult) {
            .length = length + compile_emit(self, OP_SET_ITEM, 0),
        };
    }
    else {
        return (CompileResult) {
            .length = length + compile_emit(self, OP_GET_ITEM, 0),
        };
    }
}

static CompileResult
compile_table_literal(Compiler *self, ASTTableLiteral *node) {
    ASTNode *key, *value;
    unsigned length=0, count=0;

    for (key = node->keys, value = node->values;
        key && node;
        key = key->next, value = value->next
    ) {
        length += compile_node1(self, key).length;
        length += compile_node1(self, value).length;
        count++;
    }

    return (CompileResult) {
        .length = length + compile_emit(self, OP_BUILD_TABLE, count)
    };
}
*/

static CompileResult
compile_node1(Compiler* self, ASTNode* ast, CompileResult* intermediate) {
    switch (ast->type) {
    case AST_ASSIGNMENT:
        return compile_assignment(self, (ASTAssignment*) ast);
    case AST_EXPRESSION:
        return compile_expression(self, (ASTExpression*) ast, intermediate);
    case AST_RETURN:
        return compile_return(self, (ASTReturn*) ast);
    case AST_WHILE:
        return compile_while(self, (ASTWhile*) ast);
    case AST_FOR:
    case AST_IF:
        return compile_if(self, (ASTIf*) ast);
    case AST_VAR:
        return compile_var(self, (ASTVar*) ast);
    case AST_FUNCTION:
        return compile_function(self, (ASTFunction*) ast);
    case AST_LITERAL:
        return compile_literal(self, (ASTLiteral*) ast);
    case AST_INVOKE:
        return compile_invoke(self, (ASTInvoke*) ast);
    case AST_MAGIC:
        return compile_magic(self, (ASTMagic*) ast);
    case AST_LOOKUP:
        return compile_lookup(self, (ASTLookup*) ast);
        /*
    case AST_CLASS:
        return compile_class(self, (ASTClass*) ast);
    case AST_ATTRIBUTE:
        return compile_attribute(self, (ASTAttribute*) ast);
    case AST_SLICE:
        return compile_slice(self, (ASTSlice*) ast);
    case AST_TUPLE_LITERAL:
        return compile_tuple_literal(self, (ASTTupleLiteral*) ast);
    case AST_INTERPOL_STRING:
        return compile_interpolated_string(self, (ASTInterpolatedString*) ast);
    case AST_INTERPOLATED:
        return compile_interpolated_expr(self, (ASTInterpolatedExpr*) ast);
    case AST_TABLE_LITERAL:
        return compile_table_literal(self, (ASTTableLiteral*) ast);
        */
    default:
        compile_error(self, "Unexpected AST node type");
    }
    return (CompileResult) { 0 };
}

static CompileResult
compile_node_count(Compiler *self, ASTNode *ast, unsigned *count) {
    CompileResult result;
    unsigned start = self->context->block->bytes;
    *count = 0;

    while (ast) {
        result = compile_node1(self, ast, &result);
        ast = ast->next;
        (*count)++;
    }

    result.length = self->context->block->bytes - start;
    return result;
}

static CompileResult
compile_node(Compiler *self, ASTNode* ast) {
    unsigned count;
    return compile_node_count(self, ast, &count);
}

void
compile_init(Compiler *compiler) {
    compiler->registers = (RegisterStatus) { .used = { 0 } };
    compile_push_context(compiler);
}

unsigned
compile_compile(Compiler* self, Parser* parser) {
    ASTNode* ast;
    unsigned start = self->context->block->bytes;

    if (!parser)
        return 0;

    CompileResult result;
    while ((ast = parser->next(parser))) {
        result = compile_node(self, ast);
    }

    self->context->regs_required = self->registers.high_water_mark + 1;
    self->context->result_reg = result.location == OP_VAR_LOCATE_REGISTER
        ? result.index : -1;
    return self->context->block->bytes - start;
}

static void
_compile_init_stream(Compiler *self, Stream *stream) {
    Parser _parser, *parser = &_parser;

    parser_init(parser, stream);
    compile_init(self);
    compile_compile(self, parser);
}

CodeContext*
compile_string(Compiler *self, const char * text, size_t length) {
    Stream _stream, *stream = &_stream;
    stream_init_buffer(stream, text, length);

    _compile_init_stream(self, stream);
    return self->context;
}

CodeContext*
compile_file(Compiler *self, FILE *restrict input) {
    Stream _stream, *stream = &_stream;
    stream_init_file(stream, input);

    _compile_init_stream(self, stream);
    return self->context;
}

CodeContext*
compile_ast(Compiler* self, ASTNode* node) {
    ASTNode* ast;

    compile_init(self);
    while (node) {
        compile_node(self, node);
        node = node->next;
    }

    return self->context;
}
