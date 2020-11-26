#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Compile/vm.h"
#include "repl.h"
#include "Include/Lox.h"
#include "Vendor/bdwgc/include/gc.h"

static bool
eval_repl_isdangling(CmdLoop* self, const char* code, size_t length) {
    Stream _stream, *stream = &_stream;
    Tokenizer* tokens;
    bool rv = false;
    int parens = 0, braces = 0;

    stream_init_buffer(stream, code, length);
    tokens = tokenizer_init(stream);
    if (!tokens)
        goto exit;

    Token* next;
    while ((next = tokens->next(tokens))->type != T_EOF) {
        switch (next->type) {
        case T_OPEN_PAREN:
            parens += 1;
            break;
        case T_CLOSE_PAREN:
            parens -= 1;
            break;
        case T_OPEN_BRACE:
            braces += 1;
            break;
        case T_CLOSE_BRACE:
            braces -= 1;
            break;
        default:
            break;
        }
    }

    // Ensure braces and parentheses are balanced
    rv = braces != 0 || parens != 0;

exit: 
    stream_uninit(stream);
    return rv;
}

static bool
repl_onecmd(CmdLoop *self, const char* line) {
    static Object* _ = NULL;
    if (!_)
        _ = (Object*) String_fromCharsAndSize("_", 1);

    if (strncmp(line, "EOF", 3) == 0)
        return true;

    char buffer[strlen(line) + 10];
    snprintf(buffer, sizeof(buffer), "%s", line);
    Object* result = LoxVM_evalStringWithScope(buffer, sizeof(buffer), self->scope);

    if (result && result != LoxNIL) {
        Hash_setItem(self->scope->globals, _, result);
        LoxString *S = String_fromObject(result);
        INCREF(S);
        printf("%.*s\n", S->length, S->characters);
        DECREF(S);
    }
    return false;
}

void
repl_loop(CmdLoop* self) {
    Stream *input = &self->stream;
    bool stop = false;
    const char *line, *prompt = self->prompt;

    if (self->preloop)
        self->preloop(self);
    if (self->intro)
        printf("%s\n", self->intro);

    while (!stop) {
        if (prompt)
            printf("%s", prompt);

        int start = input->pos;
        for (;;) {
            char n = input->next(input);
            if (n == '\n' || n == -1)
                break;
        }
        int length = input->pos - start;

        line = input->read(input, start, length);

        if (length == 0 || line[0] == 0) {
            line = "EOF";
            length = 3;
        }
        else {
            if (eval_repl_isdangling(self, line, length)) {
                prompt = self->prompt2;
                continue;
            }
        }

        stop = self->onecmd(self, line);
        if (self->postcmd)
            stop = self->postcmd(self, stop, line);

        prompt = self->prompt;
    }
}

void
repl_init(CmdLoop *loop) {
    VmScope *scope = GC_MALLOC(sizeof(VmScope));
    *scope = (VmScope) { .globals = Hash_new() };
    *loop = (CmdLoop) {
        .prompt = "(Lox) ",
        .prompt2 = " ...  ",
        .onecmd = repl_onecmd,
        .scope = scope,
    };

    FILE *stdin = fdopen(0, "r");
    stream_init_file_opts(&loop->stream, &(StreamInitOpts) {
        .file = stdin,
        .filename = "(stdin)",
        .readahead = false,
    });
}


