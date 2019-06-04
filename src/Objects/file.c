#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>

#include "file.h"
#include "integer.h"
#include "boolean.h"
#include "tuple.h"
#include "Lib/builtin.h"

#include "Vendor/bdwgc/include/gc.h"

static struct object_type FileType;

LoxFile*
Lox_FileOpen(const char *filename, const char *flags) {
    LoxFile* O = object_new(sizeof(LoxFile), &FileType);
    O->file = fopen(filename, flags);

    if (!O->file) {
        perror("Cannot open file");
        return LoxNIL;
    }
    O->filename = filename;
    O->isopen = true;

    return O;
}

static Object*
file_len(Object *self) {
    assert(self);
    assert(self->type == &FileType);

    struct stat st;
    stat(((LoxFile*) self)->filename, &st);
    return (Object*) Integer_fromLongLong(st.st_size);
}

// METHODS ----------------------------------

static Object*
file_read(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &FileType);

    int size;
    if (0 != Lox_ParseArgs(args, "i", &size))
        return LoxNIL;

    char *buffer = malloc(size);
    size_t length = fread(buffer, 1, size, ((LoxFile*) self)->file);

    if (length == 0) {
        free(buffer);
        // XXX: Should NIL or Undefined be used instead of an empty string?
        return (Object*) LoxEmptyString;
    }

    if (length < size && !(buffer = realloc(buffer, length))) {
        // How could this happen if (length ?< size)
        ;
    }

    return (Object*) String_fromMalloc(buffer, length);
}

static Object*
file_readline(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &FileType);

    fpos_t before, after;
    char *buffer = malloc(8192), *result;
    int error, length = 0;

    error = fgetpos(((LoxFile*)self)->file, &before);
    result = fgets(buffer, 8192, ((LoxFile*) self)->file);
    if (!error) {
        error = fgetpos(((LoxFile*)self)->file, &after);
        if (!error) {
            length = after - before;
        }
    }

    if (!error && !length) {
        free(buffer);
        // XXX: Should NIL or Undefined be used instead of an empty string?
        return (Object*) LoxEmptyString;
    }

    if (length && !realloc(buffer, length)) {
        // How could this happen if (length ?< size)
        ;
    }

    return (Object*) String_fromMalloc(buffer, length);
}

static Object*
file_write(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &FileType);

    int length, wrote;
    unsigned char *buffer;

    Lox_ParseArgs(args, "s#", &buffer, &length);

    while (length > 0) {
        wrote = fwrite(buffer, sizeof(*buffer), length,
            ((LoxFile*) self)->file);
        if (wrote < 0) {
            // TODO: Raise error
            perror("Unable to write to file");
            break;
        }
        length -= wrote;
        buffer += wrote;
    }

    return LoxNIL;
}

static Object*
file_close(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &FileType);

    ((LoxFile*) self)->isopen = false;

    return (Object*) Bool_fromBool(0 == fclose(((LoxFile*) self)->file));
}

static Object*
file_flush(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &FileType);

    return (Object*) Bool_fromBool(0 == fflush(((LoxFile*) self)->file));
}

static Object*
file_tell(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &FileType);

    if (!((LoxFile*) self)->isopen)
        return LoxUndefined;

    fpos_t pos;
    if (fgetpos(((LoxFile*)self)->file, &pos)) {
        perror("Unable to fetch file position");
    }

    return (Object*) Integer_fromLongLong(pos);
}

static Object*
file_asstring(Object *self) {
    assert(self);
    assert(self->type == &FileType);

    char buffer[256];
    int length = snprintf(buffer, sizeof(buffer), "%s file '%s'",
        ((LoxFile*) self)->isopen ? "open" : "closed",
        ((LoxFile*) self)->filename);

    return (Object*) String_fromCharArrayAndSize(buffer, length);
}

static void
file_cleanup(Object *self) {
    assert(self);
    assert(self->type == &FileType);

    LoxFile* F = (LoxFile*) self;

    if (F->file && F->isopen)
        fclose(F->file);
}

static struct object_type FileType = (ObjectType) {
    .code = TYPE_OBJECT,
    .name = "file",
    .len = file_len,

    .compare = IDENTITY,
    .hash = MYADDRESS,

    .as_string = file_asstring,

    .methods = (ObjectMethod[]) {
        { "read", file_read },
        { "readline", file_readline },
        { "write", file_write },
        { "close", file_close },
        { "flush", file_flush },
        { "tell", file_tell },
        { 0, 0 },
    },

    .cleanup = file_cleanup,
};