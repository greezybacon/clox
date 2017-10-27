#ifndef STREAM_H
#define STREAM_H

#include <stdio.h>

struct stream;

typedef struct stream_ops {
    char        (*next)(struct stream*);
    char        (*peek)(struct stream*);
    int         (*read)(struct stream*, int start, char* buffer, int length);
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
    int         (*read)(struct stream*, int start, char* buffer, int length);
    StreamOps*  ops;
} Stream;

int
stream_init(Stream*);

int
stream_init_file(Stream*, FILE*);

#endif
