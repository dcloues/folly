#ifndef RUNTIME_H
#define RUNTIME_H

#include "type.h"
#include "lexer.h"
#include "mm.h"
#include "string.h"
#include "modules/list.h"


/*
typedef struct {
	hval *top_level;
	hval *last_result;
	mem *mem;
	hval *primitive_pool;
	hval *object_root;

	lexer_input *input;
	token *current;
	token *peek;
	linked_list *loaded_modules;
} runtime;

typedef struct _native_function_spec {
	char *path;
	native_function function;
} native_function_spec;
*/

runtime *__current_runtime;

runtime *runtime_create();
void runtime_destroy();
hval *runtime_exec(runtime *runtime, lexer_input *lexer);
hval *runtime_load_module(runtime *runtime, lexer_input *lexer);
hval *runtime_exec_one(runtime *runtime, lexer_input *input, bool *terminated);
hval *runtime_eval_token(token *token, runtime *runtime, hval *context, hval *last_result);
hval *runtime_eval_identifier(token *token, runtime *runtime, hval *context);
hval *runtime_call_function(runtime *runtime, hval *fn, hval *args, hval *context);
hval *runtime_call_hnamed_function(runtime *runtime, hstr *name, hval *site, list_hval *args, hval *context);
void runtime_init_globals();
void runtime_destroy_globals();
hval *runtime_build_function_arguments(runtime *runtime, hval *fn, list_hval *in_args);

#define CURRENT_RUNTIME __current_runtime
#endif

