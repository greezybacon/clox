#include <stdlib.h>
#include <stdio.h>

#include "stream.h"

// File stream reader

typedef struct file_buffer_chunk {
    int     start;
    int     length;
    char    buffer[1024];
    struct file_buffer_chunk* prev;
    struct file_buffer_chunk* next;
} FileBufferChunk;

typedef struct {
    FILE*   file;
    int     fp;
    int     chunk_pos;
    FileBufferChunk* chunks;
} FileStream;

static int
file_stream_readahead(Stream* stream) {
    FileStream* context = (FileStream*) stream->context;
    FileBufferChunk* chunk = context->chunks;
    if (!chunk || context->chunk_pos >= (chunk->start + chunk->length)) {
        FileBufferChunk* chunk = calloc(1, sizeof(FileBufferChunk));
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

static char*
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
    char* buffer = calloc(1, length);

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

static char*
stream_read(Stream* stream, int offset, int length) {
    return stream->ops->read(stream, offset, length);
}

int
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
stream_init_file(Stream* stream, FILE* file) {
    stream_init(stream);

    stream->context = calloc(1, sizeof(FileStream));
    if (stream->context == NULL)
        // Do something...
        ;

    FileStream* fstream = (FileStream*) stream->context;
    *fstream = (FileStream) {
        .file = file,
        .chunks = NULL,
    };

    stream->ops = &file_stream_ops;

    file_stream_readahead(stream);
}
