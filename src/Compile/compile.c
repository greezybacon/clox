#include <assert.h>

#include "vm.h"
#include "compile.h"

unsigned compile_node(Compiler *self, ASTNode* ast);

static void
compile_error(Compiler* self, char* message) {
    
}

// Function compilation
static void
compile_push_context(Compiler* self) {
    CodeContext *context = malloc(sizeof(CodeContext));
    Object **constants = malloc(8 * sizeof(Object*));
    *context = (CodeContext) {
        .constants = constants,
        .sizeConstants = 8,
        .locals = (LocalsList) {
            .size = 8,
            .names = malloc(sizeof(Object*) * 8),
        },
    };
    self->context = context;
}

static CodeContext*
compile_pop_context(Compiler* self) {
    // Set the context of the compiler to be the previous, return the current
}

static unsigned
compile_emit_context(Compiler* self, CodeContext* context) {
    
}

static void
compile_start_block(Compiler *self) {
    CodeBlock *block = malloc(sizeof(CodeBlock));
    *block = (CodeBlock) {
        .size = 32,
        .prev = self->context->block,
        .instructions = calloc(32, sizeof(Instruction)),
    };
    self->context->block = block;
}

static CodeBlock*
compile_block(Compiler *self, ASTNode* node) {
    compile_start_block(self);
    compile_node(self, node);
    CodeBlock* rv = self->context->block;
    self->context->block = rv->prev;
    return rv;
}

static unsigned
compile_merge_block(Compiler *self, CodeBlock* block) {
    CodeBlock *current = self->context->block;
    while (current->size - current->nInstructions < block->nInstructions) {
        current->size *= 2;
        current->instructions = realloc(current->instructions,
            current->size * sizeof(Instruction));
    }

    Instruction *i = block->instructions;
    unsigned rv = JUMP_LENGTH(block);
    for (; block->nInstructions--; i++)
        *(current->instructions + current->nInstructions++) = *i;

    free(block->instructions);
    free(block);

    return rv;
}

static unsigned short
compile_emit_constant(Compiler *self, Object *value) {
    CodeContext *context = self->context;
    unsigned short index = 0;
    {
        Object **C = context->constants;
        while (index < context->nConstants) {
            if (Bool_isTrue(value->type->op_eq(value, *C)))
                return index;
            index++;
            C++;
        }
    }
    if (context->nConstants == context->sizeConstants) {
        context->sizeConstants *= 2;
        context->constants = realloc(context->constants,
            context->sizeConstants * sizeof(Object*));
    }
    index = context->nConstants++;
    *(context->constants + index) = value;
    return index;
}

static unsigned
compile_emit(Compiler* self, enum opcode op, short argument) {
    CodeBlock *block = self->context->block;
    if (block->nInstructions == block->size) {
        block->size *= 2;
        block->instructions = realloc(block->instructions,
            block->size * sizeof(Instruction));
    }
    *(block->instructions + block->nInstructions++) = (Instruction) {
        .op = op,
        .arg = argument,
    };
    return 1;
}

static int
compile_locals_islocal(Compiler *self, Object *name) {
    int index = 0;
    LocalsList *locals = &self->context->locals;

    // Direct search through the locals list
    while (index < locals->count) {
        if (LoxTRUE == name->type->op_eq(name, *(locals->names + index))) {
            // Already in the locals list
            return index;
        }
        index++;
    }

    return -1;
}

static unsigned
compile_locals_allocate(Compiler *self, Object *name) {
    assert(name->type);
    assert(name->type->op_eq);

    int index;
    LocalsList *locals = &self->context->locals;

    if (-1 != (index = compile_locals_islocal(self, name))) {
        return index;
    }

    // Ensure space in the index and locals
    if (locals->size < locals->count) {
        locals->size += 8;
        locals->names = realloc(locals->names, sizeof(Object*) * locals->size);
    }

    *(locals->names + locals->count) = name;
    return locals->count++;
}

static unsigned
compile_assignment(Compiler *self, ASTAssignment *assign) {
    // Push the expression
    unsigned length = compile_node(self, assign->expression);
    unsigned index;
    
    if (true) {
        // Lookup or allocate a local variable
        index = compile_locals_allocate(self, assign->name);
        length += compile_emit(self, OP_STORE_LOCAL, index);
    }
    else {
        // Non-local
        // Find the name in the constant stack
        index = compile_emit_constant(self, assign->name);
        // Push the STORE
        length += compile_emit(self, OP_STORE, index);
    }
    return length;
}

static unsigned
compile_expression(Compiler* self, ASTExpression *expr) {
    // Push the LHS
    unsigned length = compile_node(self, expr->lhs);

    // Perform the unary op
    switch (expr->unary_op) {
        // Bitwise NOT?
        case T_BANG:
        length += compile_emit(self, OP_BANG, 0);
        break;

        case T_OP_MINUS:
        case T_OP_PLUS:
        default:
        // This is really a noop
        break;
    }

    // Perform the binary op
    if (expr->binary_op) {
        length += compile_node(self, expr->rhs);
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

            default:
            compile_error(self, "Parser error ...");
        }
    }
    return length;
}

static unsigned
compile_literal(Compiler *self, ASTLiteral *node) {
    // Fetch the index of the constant
    unsigned index = compile_emit_constant(self, node->literal);
    return compile_emit(self, OP_CONSTANT, index);
}

static unsigned
compile_lookup(Compiler *self, ASTLookup *node) {
    // See if the name is in the locals list
    int index;
    if (-1 != (index = compile_locals_islocal(self, node->name))) {
        return compile_emit(self, OP_LOOKUP_LOCAL, index);
    }
    // Fetch the index of the name constant
    index = compile_emit_constant(self, node->name);
    return compile_emit(self, OP_LOOKUP, index);
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
    // Duplicate the ToS if there is an otherwise
    if (node->otherwise)
        compile_emit(self, OP_DUP_TOP, 0);

    // Compile (but not emit) the code block
    CodeBlock *block = compile_block(self, node->block);
    // Jump over the block if condition is false
    length += compile_emit(self, OP_POP_JUMP_IF_FALSE, JUMP_LENGTH(block));
    length += compile_merge_block(self, block);

    // If there's an otherwise, then emit it
    if (node->otherwise) {
        block = compile_block(self, node->otherwise);
        // Jump over the block if condition is true
        length += compile_emit(self, OP_POP_JUMP_IF_FALSE, JUMP_LENGTH(block));
        length += compile_merge_block(self, block);
    }
    return length;
}

static unsigned
compile_function(Compiler *self, ASTFunction *node) {
    // Create a new compiler context for the function's code (with new constants)
    compile_push_context(self);

    ASTNode *p;
    ASTFuncParam *param;
    unsigned index, length = 0;
    
    // Pop the arguments off the stack
    // XXX: Do this in reverse
    for (p = node->arglist; p != NULL; p = p->next) {
        assert(p->type == AST_PARAM);
        param = (ASTFuncParam*) p;
        index = compile_emit_constant(self, (Object*) String_fromCharArrayAndSize(param->name,
            param->name_length));
        length += compile_emit(self, OP_STORE, index);
    }

    // Build the code in the new context
    length += compile_node(self, node->block);

    // TODO: PUSH function pointer / name / something?
    index = compile_emit_context(self, compile_pop_context(self));

    return length;
}

static unsigned
compile_invoke(Compiler* self, ASTInvoke *node) {
    // Push the function
    unsigned length = compile_node(self, node->callable);

    // Push all the arguments
    ASTNode* a = node->args;
    unsigned count = 0;
    for (; a != NULL; a = a->next, count++)
        length += compile_node(self, a);

    // Call the function
    return length + compile_emit(self, OP_CALL, count);
}

static unsigned
_compile_node(Compiler* self, ASTNode* ast) {
    switch (ast->type) {
    case AST_ASSIGNMENT:
        return compile_assignment(self, (ASTAssignment*) ast);
    case AST_EXPRESSION:
        return compile_expression(self, (ASTExpression*) ast);
    case AST_WHILE:
        return compile_while(self, (ASTWhile*) ast);
    case AST_FOR:
    case AST_IF:
        return compile_if(self, (ASTIf*) ast);

    case AST_VAR:

    case AST_FUNCTION:
        return compile_function(self, (ASTFunction*) ast);
    case AST_LITERAL:
        return compile_literal(self, (ASTLiteral*) ast);

    case AST_INVOKE:
        return compile_invoke(self, (ASTInvoke*) ast);
    case AST_LOOKUP:
        return compile_lookup(self, (ASTLookup*) ast);

    default:
        compile_error(self, "Unexpected AST node type");
    }
    return 0;
}

unsigned
compile_node(Compiler *self, ASTNode* ast) {
    unsigned length = 0;

    while (ast) {
        length += _compile_node(self, ast);
        ast = ast->next;
    }
    return length;
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

    compile_start_block(self);
    while ((ast = parser->next(parser))) {
        length += compile_node(self, ast);
    }

    return length;
}

CodeContext*
compile_string(Compiler *self, const char * text, size_t length) {
    Stream _stream, *stream = &_stream;
    Parser _parser, *parser = &_parser;

    stream_init_buffer(stream, text, length);
    parser_init(parser, stream);
    compile_init(self);
    compile_compile(self, parser);

    return self->context;
}