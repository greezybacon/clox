#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CONSOLIDATED

// The command-line interpreter
#include "Eval/repl.c"

// The main objects
#include "Objects/boolean.c"
#include "Objects/float.c"
#include "Objects/hash.c"
#include "Objects/module.c"
#include "Objects/string.c"
#include "Objects/class.c"
#include "Objects/function.c"
#include "Objects/integer.c"
#include "Objects/object.c"
#include "Objects/tuple.c"
#include "Objects/file.c"
#include "Objects/iterator.c"
#include "Objects/list.c"

// The core libraries
#include "Lib/args.c"
#include "Lib/builtin.c"
#include "Lib/format.c"

// The parser
#include "Parse/debug_parse.c"
#include "Parse/debug_token.c"
#include "Parse/parse.c"
#include "Parse/stream.c"
#include "Parse/token.c"

// The compiler and bytecode interpreter
#include "Compile/compile.c"
#include "Compile/debug_compile.c"
#include "Compile/eval.c"
#include "Compile/scope.c"

struct arguments {
    char *cmd;
    char *input_file;
};

static void
pretty_print(Object* value) {
    if (!value || !value->type)
        return;

    printf("Result: (%s)", value->type->name);
    if (value->type->as_string) {
        LoxString* text = (LoxString*) value->type->as_string(value);
        // TODO: assert(text->type->code == TYPE_STRING);
        printf(" %.*s\n", text->length, text->characters);
    }
}

int
main(int argc, char** argv) {
    struct arguments _arguments, *arguments = &_arguments;
    *arguments = (struct arguments) {};

    int c;
    while ((c = getopt(argc, argv, "Vhc:")) != -1) {
        switch (c) {
        case 'c':
            arguments->cmd = optarg;
            break;
        case '?':
            if (optopt == 'c')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,
                    "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        default:
            abort();        }
    }

    int index;
    for (index = optind; index < argc; index++) {
        if (!arguments->input_file) {
            arguments->input_file = argv[index];
            break;
        }
    }

    Object *result = NULL;
    if (arguments->input_file) {
        FILE *file = fopen(arguments->input_file, "r");
        if (file) {
            result = vmeval_file(file);
        }
    }
    else if (arguments->cmd) {
        result = vmeval_string(arguments->cmd, strlen(arguments->cmd));
//        result = eval_string(arguments->cmd, strlen(arguments->cmd));
    }
    else {
        CmdLoop repl;
        repl_init(&repl);
        repl_loop(&repl);
    }

    if (result)
        pretty_print(result);

    return 0;
}
