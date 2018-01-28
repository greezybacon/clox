#include <stdarg.h>
#include <stdio.h>

#include "error.h"
#include "interpreter.h"

void
eval_error(Interpreter* self, char* message, ...) {
    va_list args;

    va_start(args, self);
    vfprintf(stderr, message, args);
    va_end(args);
}