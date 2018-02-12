#ifndef MODULE_H
#define MODULE_H

#include "object.h"
#include "hash.h"

typedef struct module_object {
    // Inherits from Object
    Object          base;

    HashObject*     properties;
    Object*         name;
} ModuleObject;

typedef struct module_description {
    char*       name;
    // XXX: This seems short-sighted. What about constants?
    ObjectMethod methods[];
} ModuleDescription;

Object*
Module_init(ModuleDescription* description);

#endif