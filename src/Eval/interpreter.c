#include <stdlib.h>
#include <stdio.h>

#include "interpreter.h"
#include "Objects/string.h"
#include "Objects/boolean.h"

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
    switch (ast->type) {
    case AST_STATEMENT:
    case AST_ASSIGNMENT:
        return eval_assignment(self, (ASTAssignment*) ast);
    case AST_EXPRESSION:
        return eval_expression(self, (ASTExpression*) ast);

    case AST_WHILE:
        return eval_while(self, (ASTWhile*) ast);
    case AST_FOR:
    case AST_IF:
        return eval_if(self, (ASTIf*) ast);

    case AST_VAR:

    case AST_FUNCTION:
        return eval_function(self, (ASTFunction*) ast);
    case AST_TERM:
        return eval_term(self, (ASTTerm*) ast);
    case AST_LITERAL:
        return ((ASTLiteral*) ast)->literal;

    case AST_INVOKE:
        return eval_invoke(self, (ASTInvoke*) ast);
    case AST_LOOKUP:
        return eval_lookup(self, (ASTLookup*) ast);

    case AST_CLASS:
        break;
    }
}

void
eval_init(Interpreter* eval) {
    *eval = (Interpreter) {
        .eval = eval_eval,
    };
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

    // Start with a root stack frame and a global scope
    StackFrame *stack = StackFrame_push(&ctx);
    stack->scope = Scope_create(NULL, stack->locals);

    Object* result = ctx.eval(&ctx, &parser);
    return result;
    
    fprintf(stdout, "%p\n", result->type);
}