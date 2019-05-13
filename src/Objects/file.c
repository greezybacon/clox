#include <assert.h>
#include <stdio.h>

#include "file.h"
#include "integer.h"
#include "boolean.h"
#include "tuple.h"
#include "Lib/builtin.h"

#include "Vendor/bdwgc/include/gc.h"

static struct object_type FileType;

FileObject*
Lox_FileOpen(const char *filename, const char *flags) {
    FileObject* O = object_new(sizeof(FileObject), &FileType);
    O->file = fopen(filename, flags);

    if (!O->file) {
        perror("Cannot open file");
    }
    O->filename = filename;
    O->isopen = true;

    return O;
}

static Object*
file_len(Object *self) {

}

// METHODS ----------------------------------

static Object*
file_read(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &FileType);

    int size;
    if (0 != Lox_ParseArgs(args, "i", &size))
        return LoxNIL;

    char *buffer = GC_MALLOC_ATOMIC(size);
    size_t length = fread(buffer, size, 1, ((FileObject*) self)->file);

    return (Object*) String_fromMalloc(buffer, length);
}

static Object*
file_write(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &FileType);

    int length, wrote;
    unsigned char *buffer;

    Lox_ParseArgs(args, "s#", &buffer, &length);

    printf("Recieved: %s, %d\n", buffer, length);

    while (length > 0) {
        wrote = fwrite(buffer, sizeof(*buffer), length,
            ((FileObject*) self)->file);
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

    ((FileObject*) self)->isopen = false;

    return (Object*) Bool_fromBool(0 == fclose(((FileObject*) self)->file));
}

static Object*
file_flush(VmScope *state, Object *self, Object *args) {
    assert(self);
    assert(self->type == &FileType);

    return (Object*) Bool_fromBool(0 == fflush(((FileObject*) self)->file));
}

static Object*
file_asstring(Object *self) {
    assert(self);
    assert(self->type == &FileType);

    char buffer[256];
    int length = snprintf(buffer, sizeof(buffer), "%s file '%s'",
        ((FileObject*) self)->isopen ? "open" : "closed",
        ((FileObject*) self)->filename);

    return (Object*) String_fromCharArrayAndSize(buffer, length);
}

static void
file_cleanup(Object *self) {
    assert(self);
    assert(self->type == &FileType);

    FileObject* F = (FileObject*) self;

    if (F->file && F->isopen)
        fclose(F->file);
}

static struct object_type FileType = (ObjectType) {
    .code = TYPE_OBJECT,
    .name = "file",
    .len = file_len,

    .op_eq = IDENTITY,
    .hash = MYADDRESS,

    .as_string = file_asstring,

    .methods = (ObjectMethod[]) {
        {"read", file_read},
        {"write", file_write},
        {"close", file_close},
        {"flush", file_flush},
        {0, 0},
    },

    .cleanup = file_cleanup,
};