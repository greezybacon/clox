#include <stdio.h>

#include "debug_parse.h"
#include "parse.h"

static void
print_expression(FILE* output, ASTExpression* node) {
    fprintf(output, "Expression(%d", node->unary_op);
    print_node(output, node->term);
    fprintf(output, ")");
}

static void
print_term(FILE* output, ASTTerm* node) {
    fprintf(output, "Term(%s)", node->text);
}

static void
print_call(FILE* output, ASTCall* node) {
    fprintf(output, "Call(%s, args=(", node->function_name);
    print_node(node->args);
    fprintf(output, ")");
}

static void
print_binop(FILE* output, ASTBinaryOp* node) {
    print_node(output, node->lhs);
    fprintf(output, "%d", node->op);
    print_node(output, node->rhs);
}

static void
print_var(FILE* output, ASTVar* node) {
    fprintf(output, "Var(%s=", node->name);
    print_node(node->expression);
    fprintf(output, ")");
}

static void
print_if(FILE* output, ASTIf* node) {
    fprintf(output, "If(");
    print_node(node->condition);
    fprintf(output, ") {");
    print_node(node->block);
    fprintf(output, "}");
}

static void
print_function(FILE* output, ASTFunction* node) {
    fprintf(output, "Function(%s, args=", node->name);
    print_node(output, node->arglist);
    fprintf(output, ") {");
    print_node_list(output, node->block, "\n");
    fprintf(output, "}");
}

void
print_node_list(FILE* output, ASTNode* node, const char * separator) {
    ASTNode* current = node;
    while (current) {
        switch (current->type) {
        case AST_EXPRESSION:
            return print_expression(output, (ASTExpression*) current);
        case AST_STATEMENT:
            return print_node_list(output, node, "\n");
        case AST_FUNCTION:
            return print_function(output, (ASTFunction*) current);
        case AST_WHILE:
        case AST_FOR:
        case AST_IF:
            return print_if(output, (ASTIf*) current);
        case AST_VAR:
            return print_var(output, (ASTVar*) current);
        case AST_BINARY_OP:
            return print_binop(output, (ASTBinaryOp*) current);
        case AST_TERM:
            return print_term(output, (ASTTerm*) current);
        case AST_CALL:
            return print_call(output, (ASTCall*) current);
        }

        current = node->next;
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
