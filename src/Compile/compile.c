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
        .size = 32,
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
            .names = GC_MALLOC(8 * sizeof(Constant)),
        },
        .prev = self->context,
    };
    self->context = context;
    compile_start_block(self);
}

static CodeBlock*
compile_block(Compiler *self, ASTNode* node) {
    compile_start_block(self);
    compile_node(self, node);
    CodeBlock* rv = self->context->block;
    self->context->block = rv->prev;
    return rv;
}

static inline void
compile_block_ensure_size(CodeBlock *block, unsigned size) {
    while (block->size < size) {
        block->size += 16;
        block->instructions = GC_REALLOC(block->instructions,
            block->size * sizeof(Instruction));
    }
}

static unsigned
compile_merge_block(Compiler *self, CodeBlock* block) {
    CodeBlock *current = self->context->block;

    compile_block_ensure_size(current, current->nInstructions + current->nInstructions);
    Instruction *i = block->instructions;
    unsigned rv = JUMP_LENGTH(block);
    for (; block->nInstructions--; i++)
        *(current->instructions + current->nInstructions++) = *i;

    return rv;
}

static unsigned short
compile_emit_constant(Compiler *self, Object *value) {
    CodeContext *context = self->context;
    unsigned short index = 0;

    // Short-circuit this process for emitting repeat constants
    Constant *C = context->constants;
    while (index < context->nConstants) {
        if (C->value->type == value->type
                && LoxTRUE == value->type->op_eq(value, C->value))
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

static unsigned
compile_emit(Compiler* self, enum opcode op, short argument) {
    CodeBlock *block = self->context->block;

    compile_block_ensure_size(block, block->nInstructions + 1);
    *(block->instructions + block->nInstructions++) = (Instruction) {
        .op = op,
        .arg = argument,
    };
    return 1;
}

static int
compile_locals_islocal(CodeContext *context, Object *name, hashval_t hash) {
    assert(name->type->op_eq);

    int index = 0;
    LocalsList *locals = &context->locals;

    // Direct search through the locals list
    while (index < locals->count) {
        if ((locals->names + index)->hash == hash
            && LoxTRUE == name->type->op_eq(name, (locals->names + index)->value)
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
        length += compile_emit(self, OP_STORE_LOCAL, index);
    }
    else {
        // Non-local
        // Find the name in the constant stack
        index = compile_emit_constant(self, assign->name);
        // Push the STORE
        length += compile_emit(self, OP_STORE_GLOBAL, index);
    }
    return length;
}

static unsigned
compile_expression(Compiler* self, ASTExpression *expr) {
    // Push the LHS
    unsigned length = compile_node(self, expr->lhs), rhs_length;

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
            length += compile_emit(self, OP_JUMP_IF_FALSE_OR_POP, rhs_length);
            break;

        case T_OR:
            // XXX: Length of OP_POP_TOP might not be identically 1
            length += compile_emit(self, OP_JUMP_IF_TRUE_OR_POP, rhs_length);

        default:
            break;
        }

        // Emit the RHS
        length += compile_merge_block(self, block);

        switch (expr->binary_op) {
        case T_OP_PLUS:
            length += compile_emit(self, OP_BINARY_PLUS, 0);
            break;

        case T_OP_MINUS:
            length += compile_emit(self, OP_BINARY_MINUS, 0);
            break;

        case T_OP_STAR:
            length += compile_emit(self, OP_BINARY_STAR, 0);
            break;

        case T_OP_SLASH:
            length += compile_emit(self, OP_BINARY_SLASH, 0);
            break;

        case T_OP_EQUAL:
            length += compile_emit(self, OP_EQUAL, 0);
            break;

        // case T_OP_UNEQUAL:
        case T_OP_LT:
            length += compile_emit(self, OP_LT, 0);
            break;

        case T_OP_LTE:
            length += compile_emit(self, OP_LTE, 0);
            break;

        case T_OP_GT:
            length += compile_emit(self, OP_GT, 0);
            break;

        case T_OP_GTE:
            length += compile_emit(self, OP_GTE, 0);
            break;

        case T_OP_IN:
            length += compile_emit(self, OP_IN, 0);
            break;

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
        // Bitwise NOT?
        case T_BANG:
        length += compile_emit(self, OP_BANG, 0);
        break;

        case T_OP_MINUS:
        length += compile_emit(self, OP_NEG, 0);
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

    return length + compile_emit(self, OP_BUILD_TUPLE, count);
}

static unsigned
compile_return(Compiler *self, ASTReturn *node) {
    unsigned length = 0;
    if (node->expression)
        length += compile_node(self, node->expression);

    return length + compile_emit(self, OP_RETURN, 0);
}

static unsigned
compile_literal(Compiler *self, ASTLiteral *node) {
    // Fetch the index of the constant
    unsigned index = compile_emit_constant(self, node->literal);
    return compile_emit(self, OP_CONSTANT, index);
}

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
        Object *format = String_fromLiteral(node->format, strlen(node->format));
        unsigned index = compile_emit_constant(self, format);
        length += compile_emit(self, OP_FORMAT, index);
    }

    return length;
}

static unsigned
compile_lookup(Compiler *self, ASTLookup *node) {
    // See if the name is in the locals list
    int index;
    if (-1 != (index = compile_locals_islocal(self->context, node->name, HASHVAL(node->name)))) {
        return compile_emit(self, OP_LOOKUP_LOCAL, index);
    }
    else if (-1 != (index = compile_locals_isclosed(self, node->name))) {
        return compile_emit(self, OP_LOOKUP_CLOSED, index);
    }
    // Fetch the index of the name constant
    index = compile_emit_constant(self, node->name);
    return compile_emit(self, OP_LOOKUP_GLOBAL, index);
}

static unsigned
compile_magic(Compiler *self, ASTMagic *node) {
    if (node->this)
        return compile_emit(self, OP_THIS, 0);
    else if (node->super)
        return compile_emit(self, OP_SUPER, 0);

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
    length = compile_emit(self, OP_JUMP, JUMP_LENGTH(block));
    // Do the block
    length += compile_merge_block(self, block);
    // Check the condition
    length += compile_node(self, node->condition);
    // Jump back if TRUE
    // XXX: This assumes size of JUMP is the same as POP_JUMP_IF_TRUE (probably safe)
    length += compile_emit(self, OP_POP_JUMP_IF_TRUE, -length);

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
        node->otherwise ? 1 : 0));
    // Add in the block following the condition
    length += compile_merge_block(self, block);

    // If there's an otherwise, then emit it
    if (node->otherwise) {
        block = compile_block(self, node->otherwise);
        // (As part of the positive/previous block), skip the ELSE part.
        // TODO: If the last line was RETURN, this could be ignored
        length += compile_emit(self, OP_JUMP, JUMP_LENGTH(block));
        length += compile_merge_block(self, block);
    }
    return length;
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

    // Create a constant for the function
    index = compile_emit_constant(self,
        CodeObject_fromContext(node, compile_pop_context(self)));

    // (Meanwhile, back in the original context)
    return length + compile_emit(self, OP_CONSTANT, index);
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
    length += compile_emit(self, OP_CLOSE_FUN, 0);

    // Store the named function?
    if (name) {
        if (self->flags & CFLAG_LOCAL_VARS) {
            index = compile_locals_allocate(self, name);
            length += compile_emit(self, OP_STORE_LOCAL, index);
        }
        else {
            index = compile_emit_constant(self, name);
            length += compile_emit(self, OP_STORE_GLOBAL, index);
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
            && LoxTRUE == name->type->op_eq(name, self->info->function_name)
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
        length += compile_emit(self, OP_RECURSE, node->nargs);
    else
        length += compile_emit(self, OP_CALL_FUN, node->nargs);

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
        length += compile_emit(self, OP_STORE_LOCAL, index);
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

    return length;
}

static unsigned
compile_attribute(Compiler *self, ASTAttribute *attr) {
    unsigned index,
    length = compile_node(self, attr->object);

    index = compile_emit_constant(self, attr->attribute);
    if (attr->value) {
        length += compile_node(self, attr->value);
        return length + compile_emit(self, OP_SET_ATTR, index);
    }

    return length + compile_emit(self, OP_GET_ATTR, index);
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
        return length + compile_emit(self, OP_SET_ITEM, 0);
    }
    else {
        return length + compile_emit(self, OP_GET_ITEM, 0);
    }
}

static unsigned
compile_node1(Compiler* self, ASTNode* ast) {
    switch (ast->type) {
    case AST_ASSIGNMENT:
        return compile_assignment(self, (ASTAssignment*) ast);
    case AST_EXPRESSION:
        return compile_expression(self, (ASTExpression*) ast);
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
compile_init(Compiler *compiler) {
    compile_push_context(compiler);
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

    return length;
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