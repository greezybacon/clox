#include <stdio.h>
#include <stdlib.h>

#include "parse.h"
#include "debug_parse.h"

int
eval_string(const char * text, int length) {
    Parser parser;
    parser_init(&parser, text, length);

    ASTNode* next;
    while ((next = parser.next(&parser)) != NULL) {
        print_node(stdout, next);
    }
}
