#ifndef SCOPE_H
#define SCOPE_H

#include "Objects/hash.h"

typedef struct eval_scope {
    struct eval_scope   *outer;
    HashObject          *locals;
} Scope;


Scope*
Scope_create(Scope* );

Scope*
Scope_leave(Scope* );

Object*
Scope_lookup(Scope* , Object* );

void
Scope_assign_local(Scope* , Object* , Object* );

void
Scope_assign(Scope* , Object* , Object* );

#endif