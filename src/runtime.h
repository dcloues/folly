#include "type.h"
#include "lexer.h"
#include "mm.h"
#include "string.h"

#ifndef RUNTIME_H
#define RUNTIME_H

typedef struct {
	linked_list *tokens;
	ll_node *current;
	hval *top_level;
	hval *last_result;
	mem *mem;
} runtime;

typedef struct _native_function_spec {
	char *path;
	native_function function;
} native_function_spec;

runtime *runtime_create();
void runtime_destroy();
hval *runtime_eval(runtime *runtime, char *file);
hval *runtime_eval_token(token *token, runtime *runtime, hval *context, hval *last_result);
hval *runtime_eval_identifier(token *token, runtime *runtime, hval *context);
void runtime_init_globals();
void runtime_destroy_globals();

#endif

