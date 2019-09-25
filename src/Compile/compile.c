#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

#include "vm.h"
#include "compile.h"
#include "Parse/debug_parse.h"
#include "Objects/function.h"
#include "Vendor/bdwgc/include/gc.h"

static unsigned compile_node_count(Compiler *self, ASTNode* ast, unsigned *count);
static unsigned compile_node(Compiler *self, ASTNode* ast);
static unsigned compile_node1(Compiler *self, ASTNode* ast);

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
    return rv;
}

static CodeBlock*
compile_start_block(Compiler *self) {
    CodeBlock *block = GC_MALLOC(sizeof(CodeBlock));
    *block = (CodeBlock) {
        .prev = self->context->block,
        .instructions = (InstructionList) {
            .size = 32,
            .opcodes = GC_MALLOC(32 * sizeof(Instruction)),
        },
        .codesource = (CodeSourceList) {
            .size = 8,
            .offsets = GC_MALLOC(8 * sizeof(CodeSource)),
        },
    };
    self->context->block = block;
    return block;
}

static void
compile_push_context(Compiler* self) {
    CodeContext *context = GC_MALLOC(sizeof(CodeContext));
    *context = (CodeContext) {
        .constants = GC_MALLOC(8 * sizeof(Constant)),
        .sizeConstants = 8,
        .locals = (LocalsList) {
            .size = 8,
            .names = GC_MALLOC(8 * sizeof(Constant)),
        },
        .prev = self->context,
    };
    self->context = context;
    compile_start_block(self);
}

static inline CodeBlock*
compile_pop_block(Compiler *self) {
    CodeBlock *rv = self->context->block;
    self->context->block = rv->prev;
    return rv;
}

static CodeBlock*
compile_block(Compiler *self, ASTNode* node) {
    compile_start_block(self);
    compile_node(self, node);
    return compile_pop_block(self);
}


static inline void
compile_block_ensure_size(CodeBlock *block, unsigned size) {
    InstructionList *instructions = &block->instructions;
    if (instructions->size < size) {
        while (instructions->size < size)
            instructions->size += 16;

        instructions->opcodes = GC_REALLOC(instructions->opcodes,
            instructions->size * sizeof(Instruction));
    }
}

static inline void
compile_source_ensure_size(CodeBlock *block, unsigned size) {
    if (block->codesource.size < size) {
        while (block->codesource.size < size)
            block->codesource.size += 16;

        block->codesource.offsets = GC_REALLOC(block->codesource.offsets,
            block->codesource.size * sizeof(CodeSource));
    }
}

static unsigned
compile_merge_block_into(Compiler *self, CodeBlock *dst, CodeBlock *src) {
    compile_block_ensure_size(dst, dst->instructions.count
        + src->instructions.count);
    Instruction *i = src->instructions.opcodes;
    unsigned rv = JUMP_LENGTH(src);
    for (; src->instructions.count--; i++)
        *(dst->instructions.opcodes + dst->instructions.count++) = *i;

    // And, copy the corresponding source location pointers
    CodeSource *tail = dst->codesource.offsets + dst->codesource.count - 1;
    CodeSource *head = src->codesource.offsets;
    int count = src->codesource.count;

    if (head->line_number == tail->line_number) {
        tail->opcode_count += head->opcode_count;
        head++;
        count--;
    }

    if (count) {
        compile_source_ensure_size(dst, dst->codesource.count + count);
    }

    while (count--) {
        *(dst->codesource.offsets + dst->codesource.count++) = *head;
        head++;
    }

    return rv;
}

static inline unsigned
compile_merge_block(Compiler *self, CodeBlock* block) {
    return compile_merge_block_into(self, self->context->block, block);
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

static void
compile_source_record_location(CodeBlock *block, ASTNode *node) {
    // Check if last-recorded location is still the same
    CodeSource *previous = NULL;
    if (block->codesource.count > 0) {
        previous = block->codesource.offsets + block->codesource.count - 1;
    }

    if (!node || (previous && previous->line_number == node->line)) {
        previous->opcode_count++;
        return;
    }

    // Ensure enough capacity for another source pointer
    compile_source_ensure_size(block, block->codesource.count + 1);

    // Associate the location of the source with the new opcode
    *(block->codesource.offsets + block->codesource.count++) = (CodeSource) {
        .opcode_count = 1,
        .line_number = node->line,
    };
}

static unsigned
compile_emit_into(CodeBlock *block, enum opcode op, short argument) {
    compile_block_ensure_size(block, block->instructions.count + 1);
    *(block->instructions.opcodes + block->instructions.count++) = (Instruction) {
        .op = op,
        .arg = argument,
    };

    return 1;
}

static inline unsigned
compile_emit(Compiler* self, enum opcode op, short argument, ASTNode *node) {
    unsigned length = compile_emit_into(self->context->block, op, argument);

    compile_source_record_location(self->context->block, node);

    return length;
}

static int
compile_locals_islocal(CodeContext *context, Object *name, hashval_t hash) {
    assert(name->type->compare);

    int index = 0;
    LocalsList *locals = &context->locals;

    // Direct search through the locals list
    while (index < locals->count) {
        if ((locals->names + index)->hash == hash
            && 0 == name->type->compare(name, (locals->names + index)->value)
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
        locals->names = GC_REALLOC(locals->names, sizeof(*locals->names) * locals->size);
    }

    *(locals->names + locals->count) = (Constant) {
        .value = name,
        .hash = hash,
    };
    return locals->count++;
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

static unsigned
compile_assignment(Compiler *self, ASTAssignment *assign) {
    // Push the expression
    unsigned length = compile_node(self, assign->expression);
    unsigned index;

    if (self->flags & CFLAG_LOCAL_VARS) {
        // XXX: Consider assignment to non-local (closed) vars?
        // Lookup or allocate a local variable
        index = compile_locals_allocate(self, assign->name);
        length += compile_emit(self, OP_STORE_LOCAL, index, (ASTNode*) assign);
    }
    else {
        // Non-local
        // Find the name in the constant stack
        index = compile_emit_constant(self, assign->name);
        // Push the STORE
        length += compile_emit(self, OP_STORE_GLOBAL, index, (ASTNode*) assign);
    }
    return length;
}

static struct token_to_math_op {
    enum token_type     token;
    enum opcode         vm_opcode;
    enum lox_vm_math    math_op;
    const char          *op;
} TokenToMathOp[] = {
    { T_OP_PLUS,        OP_BINARY_MATH,    MATH_BINARY_PLUS,       "+" },
    { T_OP_MINUS,       OP_BINARY_MATH,    MATH_BINARY_MINUS,      "-" },
    { T_OP_STAR,        OP_BINARY_MATH,    MATH_BINARY_STAR,       "*" },
    { T_OP_SLASH,       OP_BINARY_MATH,    MATH_BINARY_SLASH,      "/" },
    { T_OP_AMPERSAND,   OP_BINARY_MATH,    MATH_BINARY_AND,        "&" },
    { T_OP_PIPE,        OP_BINARY_MATH,    MATH_BINARY_OR,         "|" },
    { T_OP_CARET,       OP_BINARY_MATH,    MATH_BINARY_XOR,        "^" },
    { T_OP_PERCENT,     OP_BINARY_MATH,    MATH_BINARY_MODULUS,    "%" },
    { T_OP_LSHIFT,      OP_BINARY_MATH,    MATH_BINARY_LSHIFT,     "<<" },
    { T_OP_RSHIFT,      OP_BINARY_MATH,    MATH_BINARY_RSHIFT,     ">>" },
};

static int _cmpfunc (const void * a, const void * b) {
   return ((struct token_to_math_op*) a)->token - ((struct token_to_math_op*) b)->token;
}

static unsigned
compile_expression(Compiler* self, ASTExpression *expr) {
    // Push the LHS
    unsigned length = compile_node(self, expr->lhs), rhs_length;

    static bool lookup_sorted = false;
    if (!lookup_sorted) {
        qsort(TokenToMathOp, sizeof(TokenToMathOp) / sizeof(struct token_to_math_op),
            sizeof(struct token_to_math_op), _cmpfunc);
        lookup_sorted = true;
    }

    // Perform the binary op
    if (expr->binary_op) {
        // Compile the RHS in a sub-block so we can get a length to jump
        // over and emit the code after the short-circuit logic.
        CodeBlock *block = compile_block(self, expr->rhs);
        rhs_length = JUMP_LENGTH(block);

        // Handle short-circuit logic first
        switch (expr->binary_op) {
        case T_AND:
            // If the LHS is false, then jump past the end of the expression 
            // as we've already decided on the answer
            length += compile_emit(self, OP_JUMP_IF_FALSE_OR_POP, rhs_length, (ASTNode*) expr);
            break;

        case T_OR:
            // XXX: Length of OP_POP_TOP might not be identically 1
            length += compile_emit(self, OP_JUMP_IF_TRUE_OR_POP, rhs_length, (ASTNode*) expr);

        default:
            break;
        }

        // Emit the RHS
        length += compile_merge_block(self, block);

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
            struct token_to_math_op* T, key = { .token = expr->binary_op };
            T = bsearch(&key, TokenToMathOp, sizeof(TokenToMathOp) / sizeof(struct token_to_math_op),
                sizeof(struct token_to_math_op), _cmpfunc);
            length += compile_emit(self, OP_BINARY_MATH, T->math_op, (ASTNode*) expr);
            break;
        }
        case T_OP_EQUAL:
            length += compile_emit(self, OP_COMPARE, COMPARE_EQ, (ASTNode*) expr);
            break;

        case T_OP_NOTEQUAL:
            length += compile_emit(self, OP_COMPARE, COMPARE_NOT_EQ, (ASTNode*) expr);
            break;

        case T_OP_LT:
            length += compile_emit(self, OP_COMPARE, COMPARE_LT, (ASTNode*) expr);
            break;

        case T_OP_LTE:
            length += compile_emit(self, OP_COMPARE, COMPARE_LTE, (ASTNode*) expr);
            break;

        case T_OP_GT:
            length += compile_emit(self, OP_COMPARE, COMPARE_GT, (ASTNode*) expr);
            break;

        case T_OP_GTE:
            length += compile_emit(self, OP_COMPARE, COMPARE_GTE, (ASTNode*) expr);
            break;

        case T_OP_IN:
            length += compile_emit(self, OP_COMPARE, COMPARE_IN, (ASTNode*) expr);
            break;

        case T_AND:
        case T_OR:
            // already handled above
            break;

        default:
            compile_error(self, "Parser error: Unexpected binary operator: %d", expr->binary_op);
        }
    }

    return length;
}

static unsigned
compile_unary(Compiler *self, ASTUnary *node) {
    unsigned length = compile_node(self, node->expr);

    // Perform the unary op. Note that if binary and unary operations are
    // combined, then it is assumed to have been compiled from something like
    // -(a * b)
    switch (node->unary_op) {
        // Bitwise NOT?
        case T_BANG:
        length += compile_emit(self, OP_BANG, 0, (ASTNode*) node);
        break;

        case T_OP_MINUS:
        length += compile_emit(self, OP_UNARY_NEGATIVE, 0, (ASTNode*) node);
        break;

        case T_OP_PLUS:
        default:
        // This is really a noop
        break;
    }

    return length;
}

static unsigned
compile_tuple_literal(Compiler *self, ASTTupleLiteral *node) {
    unsigned length = 0, count = 0;
    ASTNode* item = node->items;

    while (item) {
        length += compile_node1(self, item);
        count++;
        item = item->next;
    }

    return length + compile_emit(self, OP_BUILD_TUPLE, count, (ASTNode*) node);
}

static unsigned
compile_return(Compiler *self, ASTReturn *node) {
    unsigned length = 0;
    if (node->expression)
        length += compile_node(self, node->expression);

    return length + compile_emit(self, OP_RETURN, 0, (ASTNode*) node);
}

static unsigned
compile_literal(Compiler *self, ASTLiteral *node) {
    // Fetch the index of the constant
    unsigned index = compile_emit_constant(self, node->literal);
    return compile_emit(self, OP_CONSTANT, index, (ASTNode*) node);
}

static unsigned
compile_interpolated_string(Compiler *self, ASTInterpolatedString *node) {
    unsigned length = 0, count;
    length += compile_node_count(self, node->items, &count);
    return length + compile_emit(self, OP_BUILD_STRING, count, (ASTNode*) node);
}

static unsigned
compile_interpolated_expr(Compiler *self, ASTInterpolatedExpr *node) {
    unsigned length;
    length = compile_node(self, node->expr);

    if (node->format) {
        LoxString *format = String_fromLiteral(node->format, strlen(node->format));
        unsigned index = compile_emit_constant(self, (Object*) format);
        length += compile_emit(self, OP_FORMAT, index, (ASTNode*) node);
    }

    return length;
}

static unsigned
compile_lookup(Compiler *self, ASTLookup *node) {
    // See if the name is in the locals list
    int index;
    if (-1 != (index = compile_locals_islocal(self->context, node->name, HASHVAL(node->name)))) {
        return compile_emit(self, OP_LOOKUP_LOCAL, index, (ASTNode*) node);
    }
    else if (-1 != (index = compile_locals_isclosed(self, node->name))) {
        return compile_emit(self, OP_LOOKUP_CLOSED, index, (ASTNode*) node);
    }
    // Fetch the index of the name constant
    index = compile_emit_constant(self, node->name);
    return compile_emit(self, OP_LOOKUP_GLOBAL, index, (ASTNode*) node);
}

static unsigned
compile_magic(Compiler *self, ASTMagic *node) {
    if (node->this)
        return compile_emit(self, OP_THIS, 0, (ASTNode*) node);
    else if (node->super)
        return compile_emit(self, OP_SUPER, 0, (ASTNode*) node);

    compile_error(self, "Unhandled magic");
    return 0;
}

static unsigned
compile_while(Compiler* self, ASTWhile *node) {
    // Emit the condition
    unsigned length = 0;
    // Emit the block in a different context, capture the length
    CodeBlock *block = compile_block(self, node->block);
    // Jump over the block
    length = compile_emit(self, OP_JUMP, JUMP_LENGTH(block), (ASTNode*) node);
    // Do the block
    length += compile_merge_block(self, block);
    // Check the condition
    length += compile_node(self, node->condition);
    // Jump back if TRUE
    // XXX: This assumes size of JUMP is the same as POP_JUMP_IF_TRUE (probably safe)
    length += compile_emit(self, OP_POP_JUMP_IF_TRUE, -length, (ASTNode*) node);

    return length;
}

static unsigned
compile_if(Compiler *self, ASTIf *node) {
    // Emit the condition
    unsigned length = compile_node(self, node->condition);

    // Compile (but not emit) the code block
    CodeBlock *block = compile_block(self, node->block);

    // Jump over the block if condition is false
    // TODO: The JUMP could perhaps be omitted (if the end of `block` is RETURN)
    // ... 1 for JUMP below (if node->otherwise)
    length += compile_emit(self, OP_POP_JUMP_IF_FALSE, JUMP_LENGTH(block) + (
        node->otherwise ? 1 : 0), (ASTNode*) node);
    // Add in the block following the condition
    length += compile_merge_block(self, block);

    // If there's an otherwise, then emit it
    if (node->otherwise) {
        block = compile_block(self, node->otherwise);
        // (As part of the positive/previous block), skip the ELSE part.
        // TODO: If the last line was RETURN, this could be ignored
        length += compile_emit(self, OP_JUMP, JUMP_LENGTH(block), (ASTNode*) node);
        length += compile_merge_block(self, block);
    }
    return length;
}

static unsigned
compile_foreach(Compiler *self, ASTForeach *node) {
    unsigned length = compile_node(self, node->iterable);
    length += compile_emit(self, OP_GET_ITERATOR, 0, (ASTNode*) node);

    CodeBlock *loop = compile_start_block(self);

    if (node->loop_var->type != AST_VAR)
        compile_error(self, "Foreach loop variable must be a `var` declaration");

    ASTVar *var = (ASTVar*) node->loop_var;
    int index = compile_locals_allocate(self,
        (Object*) String_fromCharArrayAndSize(var->name, var->name_length));

    length += compile_emit(self, OP_NEXT_OR_BREAK, index, (ASTNode*) node);

    CodeBlock *block = compile_block(self, node->block);
    compile_merge_block_into(self, loop, block);
    compile_emit(self, OP_JUMP, -JUMP_LENGTH(loop) - 1, (ASTNode*) node);

    // Now grab the loop block and return to the outer context
    compile_pop_block(self);
    length += compile_emit(self, OP_ENTER_BLOCK, JUMP_LENGTH(loop), (ASTNode*) node);
    length += compile_merge_block(self, loop);
    // And discard the iterator
    return length + compile_emit(self, OP_LEAVE_BLOCK, 1, (ASTNode*) node);
}

static unsigned
compile_control(Compiler *self, ASTControl *node) {
    if (node->loop_break) {
        return compile_emit(self, OP_BREAK, 0, (ASTNode*) node);
    }
    if (node->loop_continue) {
        return compile_emit(self, OP_CONTINUE, 0, (ASTNode*) node);
    }
}

static unsigned
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
    Compiler nested = (Compiler) {
        .context = self->context,
        .flags = self->flags | CFLAG_LOCAL_VARS,
        .info = self->info,
    };
    length += compile_node(&nested, node->block);
    if ((self->context->block->instructions.opcodes + (self->context->block->instructions.count - 1))->op != OP_RETURN)
        length += compile_emit(&nested, OP_HALT, 0, (ASTNode*) node);

    // Create a constant for the function
    index = compile_emit_constant(self,
        VmCode_fromContext(node, compile_pop_context(self)));

    // (Meanwhile, back in the original context)
    return length + compile_emit(self, OP_CONSTANT, index, (ASTNode*) node);
}

static unsigned
compile_function(Compiler *self, ASTFunction *node) {
    size_t length;
    unsigned index;
    Object *name = NULL;

    if (node->name_length) {
        name = (Object*) String_fromCharArrayAndSize(node->name, node->name_length);
        compile_push_info(self, (CompileInfo) {
            .function_name = name,
        });
    }

    length = compile_function_inner(self, node);

    // Create closure and stash in context
    length += compile_emit(self, OP_CLOSE_FUN, 0, (ASTNode*) node);

    // Store the named function?
    if (name) {
        if (self->flags & CFLAG_LOCAL_VARS) {
            index = compile_locals_allocate(self, name);
            length += compile_emit(self, OP_STORE_LOCAL, index, (ASTNode*) node);
        }
        else {
            index = compile_emit_constant(self, name);
            length += compile_emit(self, OP_STORE_GLOBAL, index, (ASTNode*) node);
        }
        compile_pop_info(self);
    }
    return length;
}

static unsigned
compile_invoke(Compiler* self, ASTInvoke *node) {
    // Push the function / callable / lookup
    unsigned length = 0;
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
    if (!recursing)
        length += compile_node(self, node->callable);

    // Push all the arguments
    if (node->nargs && node->args != NULL)
        length += compile_node(self, node->args);

    // Call the function
    if (recursing)
        length += compile_emit(self, OP_RECURSE, node->nargs, (ASTNode*) node);
    else
        length += compile_emit(self, OP_CALL_FUN, node->nargs, (ASTNode*) node);

    if (node->return_value_ignored)
        length += compile_emit(self, OP_POP_TOP, 0, (ASTNode*) node);

    return length;
}

static unsigned
compile_var(Compiler *self, ASTVar *node) {
    unsigned length = 0, index;

    // Make room in the locals for the name
    index = compile_locals_allocate(self,
        (Object*) String_fromCharArrayAndSize(node->name, node->name_length));

    if (node->expression) {
        length += compile_node(self, node->expression);
        length += compile_emit(self, OP_STORE_LOCAL, index, (ASTNode*) node);
    }

    return length;
}

static unsigned
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
            length += compile_emit(self, OP_CONSTANT, index, (ASTNode*) method);
            count++;
            compile_pop_info(self);
        }

        body = body->next;
    }

    // Inheritance
    if (node->extends) {
        length += compile_node(self, node->extends);
        length += compile_emit(self, OP_BUILD_SUBCLASS, count, (ASTNode*) node->extends);
    }
    else {
        length += compile_emit(self, OP_BUILD_CLASS, count, (ASTNode*) node);
    }

    // Support anonymous classes?
    if (node->name) {
        if (self->flags & CFLAG_LOCAL_VARS) {
            index = compile_locals_allocate(self, node->name);
            length += compile_emit(self, OP_STORE_LOCAL, index, (ASTNode*) node);
        }
        else {
            index = compile_emit_constant(self, node->name);
            length += compile_emit(self, OP_STORE_GLOBAL, index, (ASTNode*) node);
        }
    }

    return length;
}

static unsigned
compile_attribute(Compiler *self, ASTAttribute *attr) {
    unsigned index,
    length = compile_node(self, attr->object);

    index = compile_emit_constant(self, attr->attribute);
    if (attr->value) {
        length += compile_node(self, attr->value);
        return length + compile_emit(self, OP_SET_ATTR, index, (ASTNode*) attr);
    }

    return length + compile_emit(self, OP_GET_ATTR, index, (ASTNode*) attr);
}

static unsigned
compile_slice(Compiler *self, ASTSlice *node) {
    unsigned length = compile_node(self, node->object),
             index;

    if (!node->end && !node->step) {
        length += compile_node(self, node->start);
    }
    if (node->value) {
        length += compile_node(self, node->value);
        return length + compile_emit(self, OP_SET_ITEM, 0, (ASTNode*) node);
    }
    else {
        return length + compile_emit(self, OP_GET_ITEM, 0, (ASTNode*) node);
    }
}

static unsigned
compile_table_literal(Compiler *self, ASTTableLiteral *node) {
    ASTNode *key, *value;
    unsigned length=0, count=0;

    for (key = node->keys, value = node->values;
        key && node;
        key = key->next, value = value->next
    ) {
        length += compile_node1(self, key);
        length += compile_node1(self, value);
        count++;
    }

    return length + compile_emit(self, OP_BUILD_TABLE, count, (ASTNode*) node);
}

static unsigned
compile_node1(Compiler* self, ASTNode* ast) {
    switch (ast->type) {
    case AST_ASSIGNMENT:
        return compile_assignment(self, (ASTAssignment*) ast);
    case AST_EXPRESSION:
        return compile_expression(self, (ASTExpression*) ast);
    case AST_UNARY:
        return compile_unary(self, (ASTUnary*) ast);
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
    case AST_FOREACH:
        return compile_foreach(self, (ASTForeach*) ast);
    case AST_CONTROL:
        return compile_control(self, (ASTControl*) ast);
    default:
        compile_error(self, "Unexpected AST node type");
    }
    return 0;
}

static unsigned
compile_node_count(Compiler *self, ASTNode *ast, unsigned *count) {
    unsigned length = 0;
    *count = 0;

    while (ast) {
        length += compile_node1(self, ast);
        ast = ast->next;
        (*count)++;
    }
    return length;
}

static unsigned
compile_node(Compiler *self, ASTNode* ast) {
    unsigned count;
    return compile_node_count(self, ast, &count);
}

void
compile_init(Compiler *compiler, const char *stream_name) {
    compile_push_context(compiler);
    compiler->context->block->codesource.filename = stream_name;
}

unsigned
compile_compile(Compiler* self, Parser* parser) {
    ASTNode* ast;
    unsigned length = 0;

    if (!parser)
        return 0;

    while ((ast = parser->next(parser))) {
        length += compile_node(self, ast);
    }

    length += compile_emit(self, OP_HALT, 0, NULL);

    return length;
}

static void
_compile_init_stream(Compiler *self, Stream *stream) {
    Parser _parser, *parser = &_parser;

    parser_init(parser, stream);
    compile_init(self, stream->name);
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
compile_file(Compiler *self, FILE *restrict input, const char *filename) {
    Stream _stream, *stream = &_stream;
    stream_init_file(stream, input, filename);

    _compile_init_stream(self, stream);
    return self->context;
}

CodeContext*
compile_ast(Compiler* self, ASTNode* node) {
    ASTNode* ast;
    unsigned length = 0;

    compile_init(self, "(eval)");
    while (node) {
        length += compile_node(self, node);
        node = node->next;
    }

    length += compile_emit(self, OP_RETURN, 0, node);

    return self->context;
}
