#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Compile/vm.h"
#include "repl.h"
#include "Include/Lox.h"
#include "Vendor/bdwgc/include/gc.h"

static bool
eval_repl_isdangling(CmdLoop* self, char* code, size_t length) {
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
        _ = (Object*) String_fromCharArrayAndSize("_", 1);

    if (strncmp(line, "EOF", 3) == 0)
        return true;

    Object* result = vmeval_string_inscope(line, strlen(line), self->scope);

    if (result && result != LoxNIL) {
        Hash_setItem(self->scope->globals, _, result);
        LoxString *S = String_fromObject(result);
        printf("%.*s\n", S->length, S->characters);
        LoxObject_Cleanup((Object*) S);
    }
    return false;
}

void
repl_loop(CmdLoop* self) {
    char *buffer = calloc(2048, sizeof(char)), *pbuffer = buffer;

    if (self->preloop)
        self->preloop(self);
    if (self->intro)
        printf("%s\n", self->intro);

    bool stop = false;
    char *line, *prompt = self->prompt;
    while (!stop) {
        if (prompt)
            printf("%s", prompt);

        line = fgets(pbuffer, (2048 - (pbuffer - buffer)) * sizeof(char), stdin);
        if (!line) {
            strcpy(buffer, "EOF");
        }
        else {
            pbuffer += strlen(line);
            if (eval_repl_isdangling(self, buffer, pbuffer - buffer)) {
                prompt = self->prompt2;
                continue;
            }
        }

        stop = self->onecmd(self, buffer);
        if (self->postcmd)
            stop = self->postcmd(self, stop, line);

        pbuffer = buffer;
        buffer[0] = '\0';
        prompt = self->prompt;
    }
    free(buffer);
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
}


