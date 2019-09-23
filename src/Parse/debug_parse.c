#include <assert.h>
#include <stdio.h>

#include "debug_parse.h"
#include "debug_token.h"
#include "parse.h"

#include "Objects/string.h"

static void
print_expression(FILE* output, ASTExpression* node) {
    fprintf(output, "%s", "Expression(");
    print_node(output, node->lhs);
    if (node->binary_op)
        fprintf(output, " (%s) ", get_operator(node->binary_op));
    if (node->rhs)
        print_node(output, (ASTNode*) node->rhs);
    fprintf(output, ")");
}

static void
print_unary(FILE *output, ASTUnary *node) {
    if (node->unary_op)
        fprintf(output, " (%s) ", get_operator(node->unary_op));
    print_node(output, (ASTNode*) node->expr);
}

static void
print_object(FILE* output, Object *object) {
    LoxString* S;
    if (object->type->as_string) {
        S = (LoxString*) object->type->as_string(object);
        fprintf(output, "%.*s", S->length, S->characters);
    }
    else {
        fprintf(output, "object<%s>@%p", object->type->name, object);
    }
}

static void
print_assignment(FILE* output, ASTAssignment* node) {
    assert(String_isString(node->name));
    LoxString* S = (LoxString*) node->name;
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
    LoxString* S = (LoxString*) node->literal->type->as_string(node->literal);
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
    fprintf(output, "Var(%.*s=", (int) node->name_length, node->name);
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
    LoxString* S = (LoxString*) node->name;
    fprintf(output, "@%.*s", S->length, S->characters);
}

static void
print_return(FILE *output, ASTReturn* node) {
    fprintf(output, "Return ");
    print_node(output, node->expression);
}

static void
print_attribute(FILE *output, ASTAttribute *node) {
    print_node(output, node->object);
    fprintf(output, ".");
    print_object(output, node->attribute);
    if (node->value) {
        fprintf(output, " := ");
        print_node(output, node->value);
    }
}

static void
print_slice(FILE *output, ASTSlice *node) {
    print_node(output, node->object);
    fprintf(output, " [ ");
    print_node(output, node->start);
    if (node->end) {
        fprintf(output, " : ");
        print_node(output, node->end);
        if (node->step) {
            fprintf(output, " : ");
            print_node(output, node->step);
        }
    }

    fprintf(output, " ]");

    if (node->value) {
        fprintf(output, " := ");
        print_node(output, node->value);
    }
}

static void
print_class(FILE *output, ASTClass *node) {
    fprintf(output, "Class(name:=");
    print_object(output, node->name);
    if (node->extends) {
        fprintf(output, ", extends:=");
        print_node(output, node->extends);
    }
    fprintf(output, ") { ");
    print_node(output, node->body);
    fprintf(output, " }");
}

static void
print_magic(FILE *output, ASTMagic *node) {
    if (node->this)
        fprintf(output, "this");
    else if (node->super)
        fprintf(output, "super");
}

static void
print_tuple_literal(FILE *output, ASTTupleLiteral *node) {
    fprintf(output, "Tuple(");
    print_node_list(output, node->items, ", ");
    fprintf(output, ")");
}

static void
print_interpol_string(FILE *output, ASTInterpolatedString *node) {
    fprintf(output, "Concat(");
    print_node_list(output, node->items, " + ");
    fprintf(output, ")");
}

static void
print_interpol_expr(FILE *output, ASTInterpolatedExpr *node) {
    print_node(output, node->expr);
    if (node->format) {
        fprintf(output, " : %s ", node->format);
    }
}

static void
print_table_literal(FILE *output, ASTTableLiteral *node) {
    ASTNode *key, *value;
    fprintf(output, "Table(keys=(");
    print_node(output, node->keys);
    fprintf(output, "), values=(");
    print_node(output, node->values);
    fprintf(output, "))");
}

static void
print_foreach(FILE *output, ASTForeach *node) {
    fprintf(output, "Foreach(");
    print_node(output, node->loop_var);
    fprintf(output, " in ");
    print_node(output, node->iterable);
    fprintf(output, ") { ");
    print_node(output, node->block);
    fprintf(output, " }");
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
        case AST_UNARY:
            print_unary(output, (ASTUnary*) current);
            break;
        case AST_STATEMENT:
            print_node_list(output, current, "\n");
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
        case AST_ATTRIBUTE:
            print_attribute(output, (ASTAttribute*) current);
            break;
        case AST_SLICE:
            print_slice(output, (ASTSlice*) current);
            break;
        case AST_CLASS:
            print_class(output, (ASTClass*) current);
            break;
        case AST_MAGIC:
            print_magic(output, (ASTMagic*) current);
            break;
        case AST_TUPLE_LITERAL:
            print_tuple_literal(output, (ASTTupleLiteral*) current);
            break;
        case AST_INTERPOL_STRING:
            print_interpol_string(output, (ASTInterpolatedString*) current);
            break;
        case AST_INTERPOLATED:
            print_interpol_expr(output, (ASTInterpolatedExpr*) current);
            break;
        case AST_TABLE_LITERAL:
            print_table_literal(output, (ASTTableLiteral*) current);
            break;
        case AST_FOREACH:
            print_foreach(output, (ASTForeach*) current);
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
