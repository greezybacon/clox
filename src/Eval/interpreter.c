#include <stdlib.h>
#include <stdio.h>

#include "error.h"
#include "interpreter.h"
#include "Objects/string.h"
#include "Objects/boolean.h"
#include "Objects/module.h"
#include "Lib/builtin.h"

#include "Parse/debug_parse.h"

// EvalContext ---------------

static Object*
eval_eval(Interpreter* self, Parser* parser) {
    ASTNode* ast;
    Object* last = LoxNIL;

    if (!parser)
        return NULL;

    while ((ast = parser->next(parser))) {
        last = eval_node(self, ast);
    }

    return last;
}

Object*
eval_node(Interpreter* self, ASTNode* ast) {
    Object* rv = LoxNIL;

    while (ast) {
        switch (ast->type) {
        case AST_STATEMENT:
        case AST_ASSIGNMENT:
            rv = eval_assignment(self, (ASTAssignment*) ast);
            break;
        case AST_EXPRESSION:
            rv = eval_expression(self, (ASTExpression*) ast);
            break;

        case AST_WHILE:
            rv = eval_while(self, (ASTWhile*) ast);
            break;
        case AST_FOR:
        case AST_IF:
            rv = eval_if(self, (ASTIf*) ast);
            break;

        case AST_VAR:

        case AST_FUNCTION:
            rv = eval_function(self, (ASTFunction*) ast);
            break;
        case AST_TERM:
            rv = eval_term(self, (ASTTerm*) ast);
            break;
        case AST_LITERAL:
            rv = ((ASTLiteral*) ast)->literal;
            break;

        case AST_INVOKE:
            rv = eval_invoke(self, (ASTInvoke*) ast);
            break;
        case AST_LOOKUP:
            rv = eval_lookup(self, (ASTLookup*) ast);
            break;

        case AST_CLASS:
            break;

        default:
            eval_error(self, "Unexpected AST node type");
        }
        ast = ast->next;
    }

    return rv;
}

void
eval_init(Interpreter* self) {
    *self = (Interpreter) {
        .eval = eval_eval,
    };

    // Load the __builtins__ module into the global scope
    ModuleObject* builtins = BuiltinModule_init();
    Scope* globals = Scope_create(NULL, builtins->properties);

    // Start with a root stack frame and a global scope
    StackFrame *stack = StackFrame_push(self);
    stack->locals = Hash_new();
    stack->scope = Scope_create(globals, stack->locals);
}

Object*
eval_string(const char * text, size_t length) {
    Stream _stream, *stream = &_stream;
    Parser _parser, *parser = &_parser;
    Interpreter self;

    stream_init_buffer(stream, text, length);
    parser_init(parser, stream);
    eval_init(&self);

    return self.eval(&self, parser);
}

Object*
eval_file(FILE * file) {
    Stream input;
    stream_init_file(&input, file);

    Parser parser;
    parser_init(&parser, &input);

    Interpreter ctx;
    eval_init(&ctx);

    Object* result = ctx.eval(&ctx, &parser);
    return result;
    
    fprintf(stdout, "%p\n", result->type);
}