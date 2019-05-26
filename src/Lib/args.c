#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "Include/Lox.h"
#include "Objects/tuple.h"

int
Lox_ParseArgs(Object *args, const char *format, ...) {
    assert(args);
    assert(Tuple_isTuple(args));

    va_list output;
    va_start(output, format);

    Object *oArg;
    int iArg = 0, cArgs = Tuple_getSize(args);

    while (*format) {
        if (iArg >= cArgs)
            break;
        oArg = Tuple_getItem((LoxTuple*) args, iArg++);

        switch (*format) {
        case 's': {
            LoxString *string;
            if (!String_isString(oArg)) {
                if (oArg->type->as_string) {
                    string = (LoxString*) oArg->type->as_string(oArg);
                }
                else {
                    // Error
                    printf("eval: cannot coerce argument to string\n");
                    return -1;
                }
            }
            else {
                string = (LoxString*) oArg;
            }
            INCREF(string);
            *(va_arg(output, const char**)) = strndup(string->characters, string->length);
            if (*(format+1) == '#') { // 's#'
                *(va_arg(output, int*)) = string->length;
                format++;
            }
            DECREF(string);
            break;
        }

        case 'i':
        case 'I':
        case 'l':
        case 'k':
        case 'L':
        case 'K': {
            LoxInteger *value;
            if (oArg->type->as_int)
                value = (LoxInteger*) oArg->type->as_int(oArg);
            else
                // Error
                printf("eval: cannot coerce argument to integer\n");

            switch (*format) {
            case 'i':
                *(va_arg(output, int*)) = (int) value->value; break;
            case 'I':
                *(va_arg(output, unsigned int*)) = (unsigned int) value->value; break;
            case 'l':
                *(va_arg(output, long*)) = (long) value->value; break;
            case 'k':
                *(va_arg(output, unsigned long*)) = (unsigned long) value->value; break;
            case 'L':
                *(va_arg(output, long long*)) = (long long) value->value; break;
            case 'K':
                *(va_arg(output, unsigned long long*)) = (unsigned long long) value->value; break;
            }
            break;
        }

        case 'O':
            *(va_arg(output, Object**)) = oArg;
            break;
        }
        format++;
    }

    va_end(output);

    if (*format != 0) {
        printf("Too few arguments supplied to function\n");
        return -1;
    }
    else if (iArg < cArgs) {
        printf("Too many arguments supplied to function\n");
        return -1;
    }

    return 0;
}