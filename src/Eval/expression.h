#ifndef EXPRESSION_H
#define EXPRESSION_H

Object*
eval_expression(Interpreter*, ASTExpressionChain*);

Object*
eval_term(Interpreter*, ASTTerm*);

#endif