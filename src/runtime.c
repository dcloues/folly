#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "runtime.h"
#include "lexer.h"
#include "linked_list.h"
#include "type.h"
#include "ht.h"

hval *runtime_eval_loop(runtime *runtime);
hval *runtime_eval_hash(token *tok, runtime *runtime, hval *context);
hval *runtime_assignment(token *name, runtime *runtime, hval *context, hval *target);
void runtime_parse(runtime *runtime, char *file);
token *runtime_peek_token(runtime *runtime);
token *runtime_get_next_token(runtime *runtime);
static void *expect_token(token *t, type token_type);

runtime *runtime_create()
{
	runtime *r = malloc(sizeof(runtime));
	r->current = NULL;
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
	hval *context = hval_hash_create();
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
	hash_dump(context->value.hash.members, (void *) hval_to_string);

	return context;
}

hval *runtime_eval_token(token *tok, runtime *runtime, hval *context)
{
	switch(tok->type)
	{
		case identifier:
			return runtime_eval_identifier(tok, runtime, context);
		case hash_start:
			return runtime_eval_hash(tok, runtime, context);
		case string:
			return hval_string_create(tok->value.string, strlen(tok->value.string));
		case number:
			return hval_number_create(tok->value.number);
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
	hash_dump(h->value.hash.members, (char * (*)(void *))hval_to_string);
	return h;
}

hval *runtime_eval_identifier(token *tok, runtime *runtime, hval *context)
{
	token *next_token = runtime_peek_token(runtime);
	if (!runtime_peek_token(runtime))
	{
		return hash_get(context->value.hash.members, tok->value.string);
	}
	else if (next_token->type == assignment)
	{
		return runtime_assignment(tok, runtime, context, context);
	}
}

hval *runtime_assignment(token *name_token, runtime *runtime, hval *context, hval *target)
{
	// consume the assignment
	runtime_get_next_token(runtime);
	token *next_token = runtime_get_next_token(runtime);
	hval *hval = runtime_eval_token(next_token, runtime, context);
	char *name = malloc(strlen(name_token->value.string) + 1);
	strcpy(name, name_token->value.string);
	hash_put(target->value.hash.members, name, hval);
	return hval;
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
