#include "type.h"
#include "lexer.h"

#ifndef RUNTIME_H
#define RUNTIME_H

typedef struct {
	linked_list *tokens;
	ll_node *current;
} runtime;

runtime *runtime_create();
void runtime_destroy();
hval *runtime_eval(runtime *runtime, char *file);
hval *runtime_eval_token(token *token, runtime *runtime, hval *context);
hval *runtime_eval_identifier(token *token, runtime *runtime, hval *context);

#endif

