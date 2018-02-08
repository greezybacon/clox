#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "interpreter.h"
#include "repl.h"
#include "Include/Lox.h"

static bool
eval_repl_isdangling(Interpreter* self, char* code, size_t length) {
    Stream _stream, *stream = &_stream;
    Tokenizer* tokens;
    bool rv = false;

    stream_init_buffer(stream, code, length);
    tokens = tokenizer_init(stream);
    if (!tokens)
        goto exit;

    Token* next;
    while ((next = tokens->next(tokens))->type != T_EOF) {
        switch (next->type) {
        case T_OPEN_PAREN:
        case T_CLOSE_PAREN:
        case T_OPEN_BRACE:
        case T_CLOSE_BRACE:
            break;
        }
    }

    free(tokens);

exit: 
    stream_uninit(stream);
    return rv;
}

static int
eval_repl_oom(Interpreter* self) {
    
}

static int
eval_repl_next(Interpreter* self) {
    char buffer[2048], *bufpos = buffer;
    size_t chars_read;

    fprintf(stderr, ">>> ");
    for (;;) {
        bufpos += getline(&bufpos, &chars_read, stdin);
        if (bufpos - buffer >= sizeof(buffer))
            return eval_repl_oom(self);
        if (!eval_repl_isdangling(self, buffer, bufpos - buffer))
            break;
        fprintf(stderr, "... ");
    }

    Stream _stream, *stream = &_stream;
    Parser _parser, *parser = &_parser;

    stream_init_buffer(stream, buffer, bufpos - buffer);
    parser_init(parser, stream);
    self->eval(self, parser);
}

void
eval_repl(Interpreter* self) {
    for (;;) {
        eval_repl_next(self);
    }
}


static bool
repl_onecmd(CmdLoop *self, const char* line) {
    if (strncmp(line, "EOF", 3) == 0)
        return true;

    Stream _stream, *stream = &_stream;
    Parser _parser, *parser = &_parser;

    stream_init_buffer(stream, line, strlen(line));
    parser_init(parser, stream);
    Object* result = self->interpreter->eval(self->interpreter, parser);

    StringObject *S = (StringObject*) result->type->as_string(result);
    printf("%.*s\n", S->length, S->characters);

    return false;
}

void
repl_loop(CmdLoop* self) {
    char _buffer[2048], *buffer = _buffer;

    if (self->preloop)
        self->preloop(self);
    if (self->intro)
        printf("%s\n", self->intro);

    bool stop = false;
    char *line;
    while (!stop) {
        printf("%s", self->prompt);

        line = fgets(buffer, sizeof(buffer), stdin);
        if (!line)
            line = "EOF";

        stop = self->onecmd(self, line);
        if (self->postcmd)
            stop = self->postcmd(self, stop, line);
    }
}

void
repl_init(CmdLoop *loop, Interpreter* eval) {
    *loop = (CmdLoop) {
        .prompt = "(Lox) ",
        
        .onecmd = repl_onecmd,

        .interpreter = eval,
    };
}


