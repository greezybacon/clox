#ifndef BUILTINS_H
#define BUILTINS_H

#include "Objects/module.h"

ModuleObject* BuiltinModule_init(void);

// args.c
int Lox_ParseArgs(Object *, const char *, ...);

// format.c
Object* LoxObject_Format(Object *, const char *);

#endif