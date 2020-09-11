#ifndef MODULE_H
#define MODULE_H

#include "object.h"
#include "hash.h"

typedef struct module_object {
    // Inherits from Object
    Object          base;

    LoxTable*       properties;
    Object*         name;
} LoxModule;

typedef struct module_description {
    char*       name;
    // XXX: This seems short-sighted. What about constants?
    ObjectProperty properties[];
} ModuleDescription;

Object*
Module_init(ModuleDescription* description);

Object* LoxModule_FindAndImport(const char *);
int LoxModule_buildProperties(ObjectProperty*, LoxTable*);
bool LoxModule_isModule(Object*);

#endif