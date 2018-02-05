#ifndef SCOPE_H
#define SCOPE_H

#include "Objects/hash.h"

typedef struct eval_scope {
    struct eval_scope   *outer;
    HashObject          *locals;
} Scope;

#endif