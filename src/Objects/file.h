#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include "object.h"
#include "string.h"

typedef struct file_object {
    // Inherits from Object
    Object      base;

    FILE        *file;
    const char  *filename;
    bool        isopen;
} LoxFile;

LoxFile* Lox_FileOpen(const char *, const char *);

#endif
