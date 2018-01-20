#ifndef DEBUG_PARSE_H
#define DEBUG_PARSE_H

#include <stdio.h>

#include "parse.h"

void print_node(FILE*, ASTNode*);
void print_node_list(FILE*, ASTNode*, const char *);

#endif
