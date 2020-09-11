#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "exception.h"
#include "boolean.h"
#include "class.h"
#include "module.h"
#include "string.h"
#include "Compile/vm.h"
#include "Lib/builtin.h"

#include "Vendor/bdwgc/include/gc.h"

static struct object_type ExceptionType;
static struct object_type StackTraceType;

bool
Exception_isException(Object *value) {
    assert(value);
    return value->type == &ExceptionType;
}

Object*
Exception_fromConstant(const char *message, ...) {
    LoxException* O = object_new(sizeof(LoxException), &ExceptionType);
    O->message = String_fromConstant(message);
    INCREF(O->message);
    return (Object*) O;
}

Object*
Exception_fromBuffer(const char *message, size_t length) {
    LoxException* O = object_new(sizeof(LoxException), &ExceptionType);
    O->message = String_fromMalloc(message, length);
    INCREF(O->message);
    return (Object*) O;
}

Object*
Exception_fromFormatVa(const char *format, va_list args) {
    char buffer[1024];
    size_t length = vsnprintf(buffer, sizeof(buffer), format, args);

    return Exception_fromBuffer(buffer, length);
}

Object*
Exception_fromFormat(const char *format, ...) {
    va_list args;
    va_start(args, format);

    Object *rv = Exception_fromFormatVa(format, args);
    va_end(args);
    return rv;
}

Object*
Exception_fromObject(Object *object) {
    LoxException* O = object_new(sizeof(LoxException), &ExceptionType);
    O->message = String_fromObject(object);
    INCREF(O->message);
    return (Object*) O;
}

Object*
Exception_fromBaseTypeAndFormatVa(const char *format, va_list args) {
    return (Object*) LoxUndefined;
}

static Object*
exception_asstring(Object *self) {
    assert(self);
    assert(self->type == &ExceptionType);

    LoxException *this = (LoxException*) self;
    char buffer[32 + this->message->length];
    int length = snprintf(buffer, sizeof(buffer), "Exception: %.*s",
        this->message->length,
        this->message->characters);

    return (Object*) String_fromCharsAndSize(buffer, length);
}

static Object *_base = NULL;

// Exception CLASS properties
static Object*
exception__new(VmScope *state, Object *self, Object *args) {
    assert(args);

    Object *base, *message = NULL;

    // Create the base exception
    Lox_ParseArgs(args, "|O", &message);
    if (message != NULL) {
        base = Exception_fromObject(message);
    }
    else {
        base = Exception_fromConstant("Exception");
    }

    if (_base == NULL) {
        _base = (Object*) String_fromConstant("_base");
        INCREF(_base);
    }

    LoxInstance_setAttribute(self, _base, base);

    // Return value from constructor is ignored
    return LoxNIL;
}

static Object*
exception__toString(VmScope *state, Object *self, Object *args) {
    return exception_asstring(LoxInstance_getAttribute(self, _base));
}

static struct object_type ExceptionType = (ObjectType) {
    .code = TYPE_EXCEPTION,
    .name = "exception",
    .hash = MYADDRESS,

    .as_string = exception_asstring,
};

static ModuleDescription
LoxBaseExceptionDescription = {
    .name = "Exception",
    .properties = {
        { "init",       exception__new },
        { "toString",   exception__toString },
        { 0 },
    },
};

LoxClass* LoxBaseException;

__attribute__((constructor))
static void exception__startup(void) {
    LoxBaseException = LoxClass_fromModuleDescription(
        &LoxBaseExceptionDescription
    );
}

StackTrace*
StackTrace_fromContext(VmEvalContext *context) {

}

void
StackTrace_print(FILE *output, StackTrace *self) {
    
}