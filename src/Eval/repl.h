#ifndef REPL_H
#define REPL_H

typedef struct class_cmdloop {
    char        *intro;
    char        *prompt;
    char        *prompt2;
    VmScope     *scope;
    Stream      stream;

    void (*preloop)(struct class_cmdloop*);
    bool (*onecmd)(struct class_cmdloop*, const char*);
    bool (*postcmd)(struct class_cmdloop*, bool, const char*);
} CmdLoop;

void repl_init(CmdLoop*);
void repl_loop(CmdLoop*);

#endif
