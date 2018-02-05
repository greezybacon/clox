#include <stdlib.h>
#include <stdio.h>

#include "interpreter.h"
#include "Objects/string.h"
#include "Objects/boolean.h"

#include "Parse/debug_parse.h"

// EvalContext ---------------

static Object*
eval_eval(Interpreter* self) {
    ASTNode* ast;
    Parser* parser = self->parser;
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

static void
eval_init(Interpreter* eval, Parser* parser) {
    *eval = (Interpreter) {
        .parser = parser,
        .eval = eval_eval,
    };
}

int
eval_string(const char * text, int length) {
    Parser parser;
}

int
eval_stdin(void) {
    Stream input;
    stream_init_file(&input, stdin);

    Parser parser;
    parser_init(&parser, &input);

    Interpreter ctx;
    eval_init(&ctx, &parser);

    // Start with a root stack frame and a global scope
    StackFrame *stack = StackFrame_push(&ctx);
    stack->scope = Scope_create(NULL, stack->locals);

    Object* result = ctx.eval(&ctx);

    fprintf(stdout, "%p\n", result->type);

    if (result && result->type) {
        printf("Result: (%s)", result->type->name);
        if (result->type->as_string) {
            StringObject* text = (StringObject*) result->type->as_string(result);
            // TODO: assert(text->type->code == TYPE_STRING);
            printf(" %.*s\n", text->length, text->characters);
        }
    }

    /*
    ASTNode* next;
    while ((next = parser.next(&parser)) != NULL) {
        print_node(stdout, next);
        fprintf(stdout, "\n---\n");
    }
    */
}

int
main(int argc, char** argv) {
    return eval_stdin();
}
