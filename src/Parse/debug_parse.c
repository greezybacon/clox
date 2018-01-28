#include <stdio.h>

#include "debug_parse.h"
#include "debug_token.h"
#include "parse.h"

static void
print_expression_chain(FILE* output, ASTExpressionChain* node) {
    if (node->unary_op)
        fprintf(output, " (%s) ", get_operator(node->unary_op));
    fprintf(output, "%s", "Expression(");
    print_node(output, node->lhs);
    fprintf(output, ")");
    if (node->op)
        fprintf(output, " (%s)", get_operator(node->op));
    if (node->rhs)
        print_node(output, node->rhs);
}

static void
print_expression(FILE* output, ASTExpression* node) {
    return;
    fprintf(output, "%s", "Expression(");
    if (node->unary_op)
        ;
    print_node(output, node->term);
    fprintf(output, ")");
}

static void
print_term(FILE* output, ASTTerm* node) {
    fprintf(output, "Term:%s(%.*s)", get_token_type(node->token_type),
        node->length, node->text);
}

static void
print_invoke(FILE* output, ASTInvoke* node) {
    fprintf(output, "Invoke(");
    print_node(output, node->callable);
    fprintf(output, ", args=(");
    print_node(output, node->args);
    fprintf(output, ")");
}

static void
print_binop(FILE* output, ASTBinaryOp* node) {
    print_node(output, node->lhs);
    fprintf(output, " (%s) ", get_operator(node->op));
    print_node(output, node->rhs);
}

static void
print_var(FILE* output, ASTVar* node) {
    fprintf(output, "Var(%s=", node->name);
    print_node(output, node->expression);
    fprintf(output, ")");
}

static void
print_if(FILE* output, ASTIf* node) {
    fprintf(output, "If(");
    print_node(output, node->condition);
    fprintf(output, ") {");
    print_node(output, node->block);
    fprintf(output, "}");
}

static void
print_function(FILE* output, ASTFunction* node) {
    fprintf(output, "Function(name=%s, params=", node->name);
    print_node(output, node->arglist);
    fprintf(output, ") {");
    print_node_list(output, node->block, "\n");
    fprintf(output, "}");
}

void
print_node_list(FILE* output, ASTNode* node, const char * separator) {
    ASTNode* current = node;
    if (!node)
        fprintf(output, "(null)");
    else while (current) {
        switch (current->type) {
        case AST_EXPRESSION:
            print_expression(output, (ASTExpression*) current);
            break;
        case AST_EXPRESSION_CHAIN:
            print_expression_chain(output, (ASTExpressionChain*) current);
            break;
        case AST_STATEMENT:
            print_node_list(output, node, "\n");
            break;
        case AST_FUNCTION:
            print_function(output, (ASTFunction*) current);
            break;
        case AST_WHILE:
        case AST_FOR:
        case AST_IF:
            print_if(output, (ASTIf*) current);
            break;
        case AST_VAR:
            print_var(output, (ASTVar*) current);
            break;
        case AST_BINARY_OP:
            print_binop(output, (ASTBinaryOp*) current);
            break;
        case AST_TERM:
            print_term(output, (ASTTerm*) current);
            break;
        case AST_INVOKE:
            print_invoke(output, (ASTInvoke*) current);
            break;
        }

        current = current->next;
        if (current) {
            fprintf(output, "%s", separator);
        }
    }
}

void
print_node(FILE* output, ASTNode* node) {
    // TODO: Print basic info
    print_node_list(output, node, ", ");
}
