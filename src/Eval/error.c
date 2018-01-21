#include <stdio.h>

#include "error.h"
#include "interpreter.h"

void
eval_error(Interpreter* self, char* message) {
    fprintf(stderr, "%s", message);
}