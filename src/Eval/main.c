#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "interpreter.h"
#include "repl.h"
#include "Include/Lox.h"

const char *argp_program_version =
  "Lox (0.1)";
const char *argp_program_bug_address =
  "<gravydish@gmail.com>";

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
        StringObject* text = (StringObject*) value->type->as_string(value);
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
            abort();
        }
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
            result = eval_file(file);
        }
    }
    else if (arguments->cmd) {
        result = eval_string(arguments->cmd, strlen(arguments->cmd));
    }
    else {
        CmdLoop repl;
        Interpreter ctx;
        eval_init(&ctx);
        repl_init(&repl, &ctx);
        repl_loop(&repl);
    }
    
    if (result)
        pretty_print(result);

    return 0;
}