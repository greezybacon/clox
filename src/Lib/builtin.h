#ifndef BUILTINS_H
#define BUILTINS_H

ModuleObject* BuiltinModule_init(void);

// args.c
int Lox_ParseArgs(Object *, const char *, ...);

#endif