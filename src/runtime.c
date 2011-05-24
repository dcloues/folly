#include <stdlib.h>
#include <stdio.h>
#include "runtime.h"
#include "lexer.h"
#include "linked_list.h"
#include "type.h"
#include "ht.h"

hval *runtime_eval_loop(runtime *runtime);
void runtime_parse(runtime *runtime, char *file);
token *runtime_peek_token(runtime *runtime);
token *runtime_get_next_token(runtime *runtime);

runtime *runtime_create()
{
	runtime *runtime = malloc(sizeof(runtime));
	return runtime;
}

hval *runtime_eval(runtime *runtime, char *file)
{
	runtime_parse(runtime, file);

	runtime_eval_loop(runtime);
	return NULL;
}

void runtime_parse(runtime *runtime, char *file)
{
	printf("runtime: %p\n", runtime);
	FILE *fh = fopen(file, "r");
	linked_list *tokens = ll_create();
	token *tok = NULL;
	while (tok = get_next_token(fh))
	{
		ll_insert_tail(tokens, tok);
	}

	runtime->tokens = tokens;
	/*runtime->current = tokens->head;*/

	fclose(fh);
}

hval *runtime_eval_loop(runtime *runtime)
{
	printf("runtime_eval_loop\n");
	hval *context = hval_hash_create();
	printf("created context\n");
	token *tok = NULL;
	/*while (tok = runtime_get_next_token(runtime))*/
	
	while (tok = runtime_get_next_token(runtime))
	{
		printf("processing token: %p\n", tok);
		char *str = token_to_string(tok);
		printf("evaluating token: %s", str);
		/*free(str);*/
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
		case string:
			return hval_string_create(tok->value.string);
		case number:
			return hval_number_create(tok->value.number);
		default:
			printf("unhandled token\n");
	}
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
		// consume the assignment
		runtime_get_next_token(runtime);
		token *next_token = runtime_get_next_token(runtime);
		hval *hval = runtime_eval_token(next_token, runtime, context);
		char *name = tok->value.string;
		hash_put(context->value.hash.members, name, hval);
		return hval;
	}
}

token *runtime_peek_token(runtime *runtime)
{
	return runtime->current && runtime->current->next
		? runtime->current->next->data
		: NULL;
}

token *runtime_get_next_token(runtime *runtime)
{
	printf("runtime_get_next_token: %p\n", runtime);
	printf("tokens: %p\n", runtime->tokens);
	printf("current: %p\n", runtime->current);
	if (runtime->current)
	{
		printf("got a current token - skipping to next\n");
		runtime->current = runtime->current->next;
		return runtime->current ? runtime->current->data : NULL;
	}
	else
	{
		runtime->current = runtime->tokens->head;
		printf("set current to: %p\n", runtime->current);
		printf("data: %p\n", runtime->current->data);
		return runtime->current->data;
	}

	printf("down here\n");

	return NULL;
}
