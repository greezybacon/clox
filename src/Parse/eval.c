#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "eval.h"
#include "parse.h"
#include "debug_parse.h"

#include "Lib/object.h"
#include "Lib/string.h"
#include "Lib/float.h"
#include "Lib/integer.h"

static struct operator_precedence {
    enum token_type operator;
    int             precedence;
} OperatorPrecedence[] = {
    { T_OP_ASSIGN, 10 },
    { T_OP_GT, 20 },
    { T_OP_GTE, 20 },
    { T_OP_LT, 20 },
    { T_OP_LTE, 20 },
    { T_BANG, 30 },
    { T_AND, 30 },
    { T_OR, 30 },
    { T_OP_EQUAL, 40 },
    { T_OP_PLUS, 50 },
    { T_OP_MINUS, 50 },
    { T_OP_STAR, 60 },
    { T_OP_SLASH, 60 },
    { 0, 0 },
};

static inline int
get_precedence(enum token_type op) {
    struct operator_precedence *P = OperatorPrecedence,
        *Pl = NULL;
    for (; P->operator != 0; P++) {
        if (P->operator == op)
            return P->precedence;
    }
    fprintf(stdout, "Huh %d\n", op);
    return 0;
}

typedef struct stack_entry {
    struct stack_entry * next;
    Object* object;
} StackEntry;

typedef struct stack_thingy {
    struct stack_entry * head;

    Object* (*pop)(struct stack_thingy*);
    void (*push)(struct stack_thingy*, Object*);
} Stack;

static Object*
stack_pop(Stack* self) {
    if (self->head == NULL)
        return NULL;

    Object* rv = self->head->object;
    free(self->head);

    self->head = self->head->next;    
    return rv;
}

static void
stack_push(Stack* self, Object* object) {
    StackEntry* new = calloc(1, sizeof(StackEntry));
    if (new == NULL)
        return;

    *new = (StackEntry) {
        .next = self->head,
        .object = object,
    };

    self->head = new;
}

static void
stack_init(Stack* stack) {
    *stack = (Stack) {
        .pop = stack_pop,
        .push = stack_push,
        .head = NULL,
    };
}

static void
eval_error(EvalContext* self, char* message) {
    fprintf(stderr, message);
}

static Object* eval_node(EvalContext*, ASTNode*);

static Object*
eval_lookup(EvalContext* self, char* name, size_t length) {
    // TODO: Search local stack frame first
    
    // TODO: Cache the key in the AST node ...
    Object* key = (Object*) String_fromCharArrayAndSize(name, length);
    Object* value;

    HashObject* globals = self->globals;
    if ((value = ((Object*)globals)->type->get_item(globals, key))) {
        return value;
    }

    eval_error(self, "%s: Variable has not yet been set");
    return NULL;
}

static Object*
eval_binary_op(EvalContext* self, Object* arg1, Object* arg2, enum token_type op) {    
    Object* rv;

    switch (op) {
    case T_OP_PLUS:
        if (!arg2->type->op_plus)
            eval_error(self, "Object does not support `+`");
        rv = arg2->type->op_plus(arg2, arg1);
        DECREF(arg2);
        DECREF(arg1);
        break;

    case T_OP_MINUS:
        return arg2->type->op_minus(arg2, arg1);
    case T_OP_STAR:
        return arg2->type->op_star(arg2, arg1);
    case T_OP_SLASH:
        return arg2->type->op_slash(arg2, arg1);
    case T_OP_LT:
    case T_OP_LTE:
    case T_OP_GT:
    case T_OP_GTE:
    case T_OP_EQUAL:
    case T_OP_ASSIGN:
    case T_BANG:
    case T_AND:
    case T_OR:
        break;
    }
}

static Object*
eval_expression(EvalContext* self, ASTExpressionChain* expr) {
    // This uses the shunting yard algorithm here and in parsing to avoid
    // recursion issues.
    ASTExpressionChain *prev = NULL, *current = expr;

    Stack _stack, *stack = &_stack;
    stack_init(stack);

    do {
        stack->push(stack, eval_node(self, current->term));
        if (current->op) {
            current->precedence = get_precedence(current->op);
            while (prev && prev->precedence > current->precedence) {
                // Emit previous operator
                // LHS is PREV
                // RHS is CURRENT
                stack->push(stack, eval_binary_op(self, stack->pop(stack), 
                    stack->pop(stack), prev->op));
                // In the prev chain, bypass this EXPR so that it is not emitted 
                // again below
                prev = prev->prev;
            }
            current->prev = prev;
            prev = current;
        }
        current = (ASTExpressionChain*) ((ASTNode*) current)->next;
    }
    while (current);
    
    while (prev) {
        if (prev->op)
            stack->push(stack, eval_binary_op(self, stack->pop(stack),
                stack->pop(stack), prev->op));
        prev = prev->prev;
    }
    
    Object* rv = stack->pop(stack);
    assert(stack->head == NULL);

    return rv;
}

static Object*
eval_term(EvalContext* self, ASTTerm* term) {
    Object* rv;
    switch (term->token_type) {
    case T_NUMBER:
        if (term->isreal) {
            rv = (Object*) Float_fromLongDouble(term->token.real);
        }
        else {
            rv = (Object*) Integer_fromLongLong(term->token.integer);
        }
        INCREF(rv);
        break;
    case T_STRING:
        rv = (Object*) String_fromCharArrayAndSize(term->text, term->length);
        INCREF(rv);
        break;
    case T_NULL:
    // return LoxNIL;
        fprintf(stdout, "%s", "PUSH (null)\n");
        break;
    case T_TRUE:
    // return LoxTRUE;
        fprintf(stdout, "%s", "PUSH (true)\n");
        break;
    case T_FALSE:
    // return LoxFALSE;
        break;
    case T_WORD:
        return self->lookup(self, term->text, term->length);
    default:
    fprintf(stdout, "%s: %d\n%s", "Say what?", term->token_type,
        (void*) ((ASTNode*) NULL)->next);
    }

    return rv;
}

// EvalContext ---------------

static Object*
eval_eval(EvalContext* self) {
    ASTNode* ast;
    Parser* parser = self->parser;
    Object* last;
    
    if (!parser)
        return NULL;
    
    while ((ast = parser->next(parser))) {
        last = eval_node(self, ast);
    }
    
    return last;
}

static Object*
eval_node(EvalContext* self, ASTNode* ast) {
    switch (ast->type) {
    case AST_STATEMENT:
    case AST_EXPRESSION:
    case AST_EXPRESSION_CHAIN:
        return eval_expression(self, (ASTExpressionChain*) ast);

    case AST_FUNCTION:
    case AST_WHILE:
    case AST_FOR:
    case AST_IF:
    case AST_VAR:
    case AST_BINARY_OP:
    case AST_TERM:
        return eval_term(self, (ASTTerm*) ast);

    case AST_CALL:
    case AST_CLASS:
        break;
    }
}

static void
eval_init(EvalContext* eval, Parser* parser) {
    *eval = (EvalContext) {
        .parser = parser,
        .eval = eval_eval,
        .lookup = eval_lookup,
        
        .globals = Hash_new(),
        // .stack = StackFrame_new(),
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
    
    EvalContext ctx;
    eval_init(&ctx, &parser);
    
    Object* result = ctx.eval(&ctx);

    fprintf(stdout, "%p\n", result->type);

    if (result && result->type && result->type->as_string) {
        StringObject* text = result->type->as_string(result);
        fprintf(stdout, "Result: (%s) %.*s\n", result->type->name,
            text->length, text->characters);
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
