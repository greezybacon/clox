#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdarg.h>
#include <stdio.h>

#include "object.h"
#include "string.h"
#include "Compile/vm.h"

typedef struct exception_object {
    // Inherits from Object
    union {
        Object      object;
    };

    LoxString       *message;
    VmScope         scope;
} LoxException;

typedef struct typed_exception_object {
    union {
        Object      object;
        LoxException exception;
    };

    LoxClass        *extended_class;
} LoxTypedException;

typedef struct stackframe_object {
    // Inherits from Object
    Object      base;
    struct stackframe_object *next;
} StackFrame;

typedef struct stacktrace_object {
    // Inherits from Object
    Object      base;

    StackFrame *frames;
    VmEvalContext *context;
} StackTrace;

bool Exception_isException(Object *);
Object* Exception_fromMessage(const char *);
Object* Exception_fromBuffer(const char *, size_t);
Object* Exception_fromConstant(const char *, ...);
Object* Exception_fromFormatVa(const char *, va_list);
Object* Exception_fromFormat(const char *, ...);
Object* Exception_fromObject(Object *);

StackTrace* StackTrace_fromContext(VmEvalContext*);
void StackTrace_print(FILE*, StackTrace*);

// Base exception type classes
LoxClass* LoxBaseException;

#endif
