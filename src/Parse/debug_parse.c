#include <assert.h>
#include <stdio.h>

#include "debug_parse.h"
#include "debug_token.h"
#include "parse.h"

#include "Objects/string.h"

static void
print_expression(FILE* output, ASTExpression* node) {
    fprintf(output, "%s", "Expression(");
    if (node->unary_op)
        fprintf(output, " (%s) ", get_operator(node->unary_op));
    print_node(output, node->lhs);
    if (node->binary_op)
        fprintf(output, " (%s) ", get_operator(node->binary_op));
    if (node->rhs)
        print_node(output, (ASTNode*) node->rhs);
    fprintf(output, ")");
}

static void
print_assignment(FILE* output, ASTAssignment* node) {
    assert(String_isString(node->name));
    StringObject* S = (StringObject*) node->name;
    fprintf(output, "%.*s := ", S->length, S->characters);
    print_node(output, node->expression);
}

static void
print_term(FILE* output, ASTTerm* node) {
    fprintf(output, "Term:%s(%.*s)", get_token_type(node->token_type),
        node->length, node->text);
}

static void
print_literal(FILE* output, ASTLiteral* node) {
    StringObject* S = (StringObject*) node->literal->type->as_string(node->literal);
    fprintf(output, "(%.*s)", S->length, S->characters);
}

static void
print_invoke(FILE* output, ASTInvoke* node) {
    fprintf(output, "Invoke(");
    print_node(output, node->callable);
    fprintf(output, ", args=(");
    print_node(output, node->args);
    fprintf(output, "))");
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

    if (node->otherwise) {
        fprintf(output, ", Else(");
        print_node(output, node->otherwise);
        fprintf(output, ")");
    }
}

static void
print_while(FILE* output, ASTWhile* node) {
    fprintf(output, "While(");
    print_node(output, node->condition);
    fprintf(output, ") {");
    print_node(output, node->block);
    fprintf(output, "}");
}

static void
print_function(FILE* output, ASTFunction* node) {
    fprintf(output, "Function(name=%s, params={", node->name);
    print_node(output, node->arglist);
    fprintf(output, "}) { ");
    print_node_list(output, node->block, "\n");
    fprintf(output, " }");
}

static void
print_param(FILE* output, ASTFuncParam* node) {
    fprintf(output, "<%.*s>", node->name_length, node->name);
    if (node->default_value)
        print_node(output, node->default_value);
}

static void
print_lookup(FILE* output, ASTLookup* node) {
    assert(String_isString(node->name));
    StringObject* S = (StringObject*) node->name;
    fprintf(output, "@%.*s", S->length, S->characters);
}

static void
print_return(FILE *output, ASTReturn* node) {
    fprintf(output, "Return ");
    print_node(output, node->expression);
}

void
print_node_list(FILE* output, ASTNode* node, const char * separator) {
    ASTNode* current = node;
    if (!node)
        fprintf(output, "(null)");
    else while (current) {
        switch (current->type) {
        case AST_ASSIGNMENT:
            print_assignment(output, (ASTAssignment*) current);
            break;
        case AST_EXPRESSION:
            print_expression(output, (ASTExpression*) current);
            break;
        case AST_STATEMENT:
            print_node_list(output, node, "\n");
            break;
        case AST_FUNCTION:
            print_function(output, (ASTFunction*) current);
            break;
        case AST_PARAM:
            print_param(output, (ASTFuncParam*) current);
            break;
        case AST_WHILE:
            print_while(output, (ASTWhile*) current);
            break;
        case AST_FOR:
        case AST_IF:
            print_if(output, (ASTIf*) current);
            break;
        case AST_VAR:
            print_var(output, (ASTVar*) current);
            break;
        case AST_TERM:
            print_term(output, (ASTTerm*) current);
            break;
        case AST_LITERAL:
            print_literal(output, (ASTLiteral*) current);
            break;
        case AST_INVOKE:
            print_invoke(output, (ASTInvoke*) current);
            break;
        case AST_LOOKUP:
            print_lookup(output, (ASTLookup*) current);
            break;
        case AST_RETURN:
            print_return(output, (ASTReturn*) current);
            break;
        default:
            fprintf(output, "Unexpected AST type: %d", current->type);
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
