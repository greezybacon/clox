#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "interpreter.h"
#include "repl.h"
#include "Compile/compile.h"
#include "Include/Lox.h"

#include "Vendor/bdwgc/include/gc.h"

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

    GC_INIT();

    Object *result = NULL;
    if (arguments->input_file) {
        FILE *file = fopen(arguments->input_file, "r");
        if (file) {
            result = LoxVM_evalFile(file, arguments->input_file);
        }
    }
    else if (arguments->cmd) {
        result = LoxVM_evalString(arguments->cmd, strlen(arguments->cmd));
//        result = eval_string(arguments->cmd, strlen(arguments->cmd));
    }
    else {
        CmdLoop repl;
        repl_init(&repl);
        repl_loop(&repl);
    }
    
    if (result)
        pretty_print(result);
    else
        printf("NULL\n");

    return 0;
}
