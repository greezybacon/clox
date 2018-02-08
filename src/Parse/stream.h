#ifndef STREAM_H
#define STREAM_H

#include <stdio.h>

struct stream;

typedef struct stream_ops {
    char        (*next)(struct stream*);
    char        (*peek)(struct stream*);
    char*       (*read)(struct stream*, int start, int length);
    void        (*cleanup)(struct stream*);
} StreamOps;

typedef struct stream {
    void*       context;

    int         line;           // Current line number
    int         offset;         // Current char of current line
    int         pos;            // Position in stream
    char        current;

    char*       name;           // Name of the stream (STDIN or filename)
    char        (*next)(struct stream*);
    char        (*peek)(struct stream*);
    char*       (*read)(struct stream*, int start, int length);
    StreamOps*  ops;
} Stream;

int
stream_init(Stream*);

int
stream_init_file(Stream*, FILE*);

int
stream_init_buffer(Stream*, const char*, size_t);
    
void
stream_uninit(Stream *);

#endif
