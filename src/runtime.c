#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "runtime.h"
#include "lexer.h"
#include "linked_list.h"
#include "type.h"
#include "ht.h"
#include "str.h"

hval *runtime_eval_loop(runtime *runtime);
hval *runtime_eval_hash(token *tok, runtime *runtime, hval *context);
hval *runtime_assignment(token *name, runtime *runtime, hval *context, hval *target);
void runtime_parse(runtime *runtime, char *file);
token *runtime_peek_token(runtime *runtime);
token *runtime_get_next_token(runtime *runtime);
static void *expect_token(token *t, type token_type);
static void register_top_level(runtime *);
static hval *runtime_eval_list(runtime *runtime, hval *context);
static hval *runtime_apply_function(runtime *runtime, hval *function, hval *arguments);

static hval *native_print(hval *this, hval *args);

typedef struct {
	char *name;
	native_function fn;
} native_function_declaration;

static native_function_declaration top_levels[] = {
	{"print", native_print},
	{NULL, NULL}
};
	

runtime *runtime_create()
{
	runtime *r = malloc(sizeof(runtime));
	r->current = NULL;
	r->top_level = hval_hash_create();
	fprintf(stderr, "top_level: %p\n", r->top_level);
	register_top_level(r);
	return r;
}

void runtime_destroy(runtime *r)
{
	if (r)
	{ 
		if (r->tokens)
		{
			ll_destroy(r->tokens, (destructor) token_destroy);
		}
		free(r);
	}
}

static void register_top_level(runtime *r)
{
	perror("Registering top levels\n");
	int i = 0;
	while (true)
	{
		native_function_declaration *f = top_levels + i;
		++i;
		if (f->name == NULL) {
			break;
		}

		printf("registering top level function: %s\n", f->name);
		hstr *name = hstr_create(f->name);
		hval *fun = hval_native_function_create(f->fn);
		hval_hash_put(r->top_level, name, fun);
		hstr_release(name);
	}
}

hval *runtime_eval(runtime *runtime, char *file)
{
	runtime_parse(runtime, file);

	hval *context = runtime_eval_loop(runtime);
	return context;
}

void runtime_parse(runtime *runtime, char *file)
{
	FILE *fh = fopen(file, "r");
	linked_list *tokens = ll_create();
	token *tok = NULL;
	while (tok = get_next_token(fh))
	{
		ll_insert_tail(tokens, tok);
	}

	runtime->tokens = tokens;

	fclose(fh);
}

hval *runtime_eval_loop(runtime *runtime)
{
	hval *context = hval_hash_create_child(runtime->top_level);
	token *tok = NULL;
	
	while (tok = runtime_get_next_token(runtime))
	{
		printf("processing token: %p\n", tok);
		char *str = token_to_string(tok);
		printf("evaluating token: %s\n", str);
		free(str);
		runtime_eval_token(tok, runtime, context);
	}

	printf("runtime_eval_loop terminating. context:\n");
	hash_dump(context->value.hash.members, (void *)hstr_to_str, (void *) hval_to_string);

	return context;
}

hval *runtime_eval_token(token *tok, runtime *runtime, hval *context)
{
#ifdef DEBUG_EVAL_TOKEN
	char *s = token_to_string(tok);
	printf("runtime_eval_token: %s\n", s);
	free(s);
#endif
	switch(tok->type)
	{
		case identifier:
			return runtime_eval_identifier(tok, runtime, context);
		case hash_start:
			return runtime_eval_hash(tok, runtime, context);
		case string:
			return hval_string_create(tok->value.string);
		case number:
			return hval_number_create(tok->value.number);
		case list_start:
			return runtime_eval_list(runtime, context);
		default:
			printf("unhandled token\n");
	}
}

hval *runtime_eval_hash(token *tok, runtime *runtime, hval *context)
{
	hval *h = hval_hash_create();
	while (true)
	{
		tok = runtime_get_next_token(runtime);
		if (tok->type == hash_end)
		{
			break;
		}

		expect_token(tok, identifier);
		// TODO pass a separate lookup context so variables can be resolved
		runtime_assignment(tok, runtime, context, h);
	}

	printf("runtime_eval_hash\n");
	hash_dump(h->value.hash.members, (char * (*)(void *))hstr_to_str, (char * (*)(void *))hval_to_string);
	return h;
}

static hval *runtime_eval_list(runtime *runtime, hval *context)
{
	hval *list = hval_list_create();
	token *t = runtime_get_next_token(runtime);
	linked_list *l = list->value.list;
	while (t->type != list_end)
	{
		ll_insert_tail(l, runtime_eval_token(t, runtime, context));
		t = runtime_get_next_token(runtime);
	}

	return list;
}

hval *runtime_eval_identifier(token *tok, runtime *runtime, hval *context)
{
	token *next_token = runtime_peek_token(runtime);
	if (!runtime_peek_token(runtime))
	{
		return hval_hash_get(context, tok->value.string);
	}
	else if (next_token->type == assignment)
	{
		return runtime_assignment(tok, runtime, context, context);
	}
	else if (next_token->type == list_start)
	{
		hval *function = hval_hash_get(context, token_string(tok));
		runtime_get_next_token(runtime);
		hval *arguments = runtime_eval_list(runtime, context);
		if (function == NULL)
		{
			fprintf(stderr, "no function %s\n", token_string(tok)->str);
			return NULL;
		}
		hval *result = runtime_apply_function(runtime, function, arguments);

		return result;
	}
}

hval *runtime_assignment(token *name_token, runtime *runtime, hval *context, hval *target)
{
	// consume the assignment
	runtime_get_next_token(runtime);
	token *next_token = runtime_get_next_token(runtime);
	hval *value = runtime_eval_token(next_token, runtime, context);
	/*char *name = malloc(strlen(name_token->value.string) + 1);*/

	/*strcpy(name, name_token->value.string);*/
	hval_hash_put(target, token_string(name_token), value);
	return value;
}

static hval *runtime_apply_function(runtime *runtime, hval *function, hval *arguments)
{
	if (function->type != native_function_t)
	{
		perror("unexpected type");
		exit(1);
	}

	return function->value.native_fn(NULL, arguments);
}

token *runtime_peek_token(runtime *runtime)
{
	return runtime->current && runtime->current->next
		? runtime->current->next->data
		: NULL;
}

token *runtime_get_next_token(runtime *runtime)
{
	if (runtime->current)
	{
		runtime->current = runtime->current->next;
		return runtime->current ? runtime->current->data : NULL;
	}
	else
	{
		runtime->current = runtime->tokens->head;
		return runtime->current->data;
	}

	return NULL;
}

static void *expect_token(token *t, type token_type)
{
	if (!t || token_type != t->type)
	{
		fprintf(stderr, "Error: unexpected %s (expected %s)",
			token_type_string_token(t),
			token_type_string(token_type));
	}
}

static hval *native_print(hval *this, hval *args)
{
	fprintf(stderr, "IN NATIVE PRINT!\n");
	char *str = hval_to_string(args);
	puts(str);
	free(str);
	return NULL;
}

