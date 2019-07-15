#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "stream.h"
#include "Vendor/bdwgc/include/gc.h"

// File stream reader

typedef struct file_buffer_chunk {
    int     start;
    int     length;
    char    buffer[1024];
    struct file_buffer_chunk* prev;
    struct file_buffer_chunk* next;
} FileBufferChunk;

typedef struct {
    FILE* restrict file;
    int     fp;
    int     chunk_pos;
    FileBufferChunk* chunks;
} FileStream;

static int
file_stream_readahead(Stream* stream) {
    FileStream* context = (FileStream*) stream->context;
    FileBufferChunk* chunk = context->chunks;
    if (!chunk || context->chunk_pos >= (chunk->start + chunk->length)) {
        FileBufferChunk* chunk = GC_MALLOC(sizeof(FileBufferChunk));
        if (chunk == NULL)
            // PROBLEM
            ;

        chunk->start = stream->pos;

       // Link the current and new chunks together
        if (context->chunks) {
            chunk->prev = context->chunks;
            context->chunks->next = chunk;
        }
        context->chunks = chunk;

        chunk->length = fread(chunk->buffer, 1, sizeof(chunk->buffer),
            context->file);
        context->chunk_pos = 0;
    }
    return 0;
}

static char
file_stream_next(Stream* stream) {
    FileStream* context = (FileStream*) stream->context;
    file_stream_readahead(stream);

    if (context->chunks->length == 0)
        return -1;

    FileBufferChunk* chunk = context->chunks;
    char current = chunk->buffer[context->chunk_pos++];
    if (current == '\n') {
        stream->line++;
        stream->offset=0;
    }
    stream->offset++;
    stream->pos++;

    return current;
}

static char
file_stream_peek(Stream* stream) {
    FileStream* context = (FileStream*) stream->context;

    file_stream_readahead(stream);

    if (context->chunks->length == 0)
        return -1;

    FileBufferChunk* chunk = context->chunks;
    return chunk->buffer[context->chunk_pos];
}

static const char*
file_stream_readchunk(Stream* stream, int offset, int length) {
    FileStream* context = (FileStream*) stream->context;
    FileBufferChunk* chunk = context->chunks;

    // Rewind the chunks buffer back to the chunk with the offset
    while (chunk && (chunk->start > offset))
        chunk = chunk->prev;

    if (!chunk)
        return 0;

    int chunk_pos = offset - chunk->start;
    int chunk_end = chunk->start + chunk->length;

    // Try and return a pointer into the existing chunk (rather than 
    // allocating a new buffer)
    if (offset + length <= chunk_end) {
        return chunk->buffer + chunk_pos;
    }

    // Allocate a buffer to return
    // TODO: Add a way to manage the pointer and indicate that it should 
    //       be freed later
    char* buffer = GC_MALLOC_ATOMIC(length);

    int copied = 0, requested = length;
    char* bstart = buffer;
    while (length--) {
        *buffer++ = chunk->buffer[chunk_pos++];
        if (chunk_pos > chunk_end) {
            if (!chunk->next)
                break;
            chunk = chunk->next;
            chunk_end = chunk->start + chunk->length;
            chunk_pos = 0;
        }
        copied++;
    }
    return bstart;
}

static StreamOps file_stream_ops = {
    .next = file_stream_next,
    .peek = file_stream_peek,
    .read = file_stream_readchunk,
};

static char
stream_peek(Stream* stream) {
    return stream->ops->peek(stream);
}

static char
stream_next(Stream* stream) {
    return stream->ops->next(stream);
}

static const char*
stream_read(Stream* stream, int offset, int length) {
    return stream->ops->read(stream, offset, length);
}

void
stream_init(Stream* stream) {
    *stream = (Stream) {
        .line = 1,
        .offset = 1,
        .pos = 0,
        .peek = stream_peek,
        .next = stream_next,
        .read = stream_read,
    };
}

int
stream_init_file(Stream* stream, FILE* restrict file, const char *filename) {
    stream_init(stream);

    stream->context = GC_MALLOC(sizeof(FileStream));
    if (stream->context == NULL)
        // Do something...
        ;

    FileStream* fstream = (FileStream*) stream->context;
    *fstream = (FileStream) {
        .file = file,
        .chunks = NULL,
    };

    stream->ops = &file_stream_ops;
    stream->name = GC_STRNDUP(filename, strlen(filename));
    return file_stream_readahead(stream);
}

void
stream_uninit(Stream* stream) {
    if (stream->ops->cleanup)
        stream->ops->cleanup(stream);
}



typedef struct {
    const char*     buffer;
    size_t          length;
} BufferStream;

static char
buffer_stream_next(Stream* stream) {
    BufferStream* context = (BufferStream*) stream->context;

    if (stream->pos >= context->length)
        return -1;

    char current = *(context->buffer + stream->pos++);
    if (current == '\n') {
        stream->line++;
        stream->offset=0;
    }
    stream->offset++;

    return current;
}

static char
buffer_stream_peek(Stream* stream) {
    BufferStream* context = (BufferStream*) stream->context;

    if (stream->pos >= context->length)
        return -1;

    return *(context->buffer + stream->pos);
}

static const char*
buffer_stream_read(Stream* stream, int offset, int length) {
    assert(offset >= 0);
    assert(length >= 0);

    BufferStream* context = (BufferStream*) stream->context;
    
    if (offset + length > context->length)
        return NULL;

    return context->buffer + offset;
}

static StreamOps buffer_stream_ops = {
    .next = buffer_stream_next,
    .peek = buffer_stream_peek,
    .read = buffer_stream_read,
};

int
stream_init_buffer(Stream* stream, const char* buffer, size_t length) {
    stream_init(stream);

    stream->context = GC_MALLOC(sizeof(BufferStream));
    if (stream->context == NULL)
        // Do something...
        ;

    BufferStream* fstream = (BufferStream*) stream->context;
    *fstream = (BufferStream) {
        .buffer = buffer,
        .length = length,
    };

    stream->ops = &buffer_stream_ops;
    return 0;   
}