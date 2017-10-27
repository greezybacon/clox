#include <stdio.h>
#include <stdlib.h>

#include "parse.h"
#include "debug_parse.h"

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

    ASTNode* next;
    while ((next = parser.next(&parser)) != NULL) {
        print_node(stdout, next);
    }
}

int
main(int argc, char** argv) {
    return eval_stdin();
}
