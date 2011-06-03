#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "runtime.h"
#include "lexer.h"
#include "linked_list.h"
#include "log.h"
#include "type.h"
#include "ht.h"
#include "str.h"

hval *runtime_eval_loop(runtime *runtime);
hval *runtime_eval_hash(token *tok, runtime *runtime, hval *context);
hval *runtime_assignment(token *name, runtime *runtime, hval *context, hval *target);
void runtime_parse(runtime *runtime, char *file);
expression *runtime_analyze(runtime *);
token *runtime_peek_token(runtime *runtime);
token *runtime_get_next_token(runtime *runtime);
static void *expect_token(token *t, type token_type);
static void register_top_level(runtime *);
static hval *runtime_eval_list(runtime *runtime, hval *context);
static hval *runtime_apply_function(runtime *runtime, hval *function, hval *arguments);

static expression *expr_create(expression_type);
static void expr_destroy(expression *expr);
static void prop_ref_destroy(prop_ref *ref);
static expression *read_complete_expression(runtime *);
static expression *read_identifier(runtime *);
static expression *read_number(runtime *);
static expression *read_string(runtime *);

static hval *runtime_evaluate_expression(runtime *, expression *, hval *);
static hval *eval_prop_ref(runtime *, prop_ref *, hval *);
static hval *eval_prop_set(runtime *, prop_set *, hval *);
static hval *eval_expr_list(runtime *, linked_list *, hval *);

static hval *get_prop_ref_site(runtime *, prop_ref *, hval *);

static hval *native_print(hval *this, hval *args);
#define runtime_current_token(rt) ((token *) rt->current->data)

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
	hlog("top_level: %p\n", r->top_level);
	register_top_level(r);
	return r;
}

void runtime_destroy(runtime *r)
{
	if (r)
	{ 
		if (r->last_result)
		{
			/*hval_release(r->last_result);*/
		}

		if (r->tokens)
		{
			ll_destroy(r->tokens, (destructor) token_destroy);
		}

		hval_release(r->top_level);
		r->top_level = NULL;
		free(r);
	}
}

static void register_top_level(runtime *r)
{
	hlog("Registering top levels\n");
	int i = 0;
	
	hval *io = hval_hash_create();
	hstr *io_str = hstr_create("io");
	hval_hash_put(r->top_level, io_str, io);
	hval_release(io);
	hstr_release(io_str);
	io_str = NULL;

	hval *print = hval_native_function_create(native_print);
	hstr *str = hstr_create("print");
	hval_hash_put(io, str, print);

	hstr_release(str);
	str = NULL;
	hval_release(print);
	print = NULL;
}

hval *runtime_eval(runtime *runtime, char *file)
{
	runtime_parse(runtime, file);
	// this will have type=expr_list_t
	expression *expr = runtime_analyze(runtime);
	hval *context = hval_hash_create_child(runtime->top_level);
	hval *ret = runtime_evaluate_expression(runtime, expr, context);
	expr_destroy(expr);
	expr = NULL;
	hval_destroy(ret);

	/*hval *context = runtime_eval_loop(runtime);*/
	/*return context;*/
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

expression *runtime_analyze(runtime *rt)
{
	token *t = NULL;
	expression *expr_list = malloc(sizeof(expression));
	if (expr_list == NULL)
	{
		perror("Unable to allocate memory for expr_list");
		exit(1);
	}

	expr_list->type = expr_list_t;
	expr_list->operation.expr_list = ll_create();

	expression *expr = NULL;
	while ((t = runtime_get_next_token(rt)) != NULL)
	{
		expr = read_complete_expression(rt);
		if (expr == NULL)
		{
			hlog("got a null expression - failing");
			exit(1);
		}
		ll_insert_tail(expr_list->operation.expr_list, expr);
	}

	return expr_list;
}

expression *read_complete_expression(runtime *rt)
{
	expression *expr = NULL;
	token_type tt = ((token *) rt->current->data)->type;

	switch (tt)
	{
		case identifier:
			expr = read_identifier(rt);
			break;
		case number:
			expr = read_number(rt);
			break;
		case string:
			expr = read_string(rt);
			break;
	}

	return expr;
}

expression *read_identifier(runtime *rt)
{
	expression *expr = NULL;

	token *t = rt->current->data;
	prop_ref *ref = malloc(sizeof(prop_ref));
	if (ref == NULL)
	{
		perror("read_identifier unable to allocate memory for prop_ref");
		exit(1);
	}

	ref->name = t->value.string;
	ref->site = NULL;
	hstr_retain(t->value.string);

	token *next = runtime_peek_token(rt);
	if (next->type == assignment) {
		// consume the assignment and advance to the next
		runtime_get_next_token(rt);
		runtime_get_next_token(rt);
		expression *assgn = expr_create(expr_prop_set_t);
		assgn->operation.prop_set = malloc(sizeof(prop_set));
		assgn->operation.prop_set->ref = ref;
		assgn->operation.prop_set->value = read_complete_expression(rt);
		expr = assgn;
	} else if (next->type == dereference) {
		// consume the assignment and advance to the next
		runtime_get_next_token(rt);
		runtime_get_next_token(rt);
		expression *deref = read_complete_expression(rt);

		expression *deref_site = expr_create(expr_prop_ref_t);
		deref_site->operation.prop_ref = ref;
	
		deref->operation.prop_ref->site = deref_site;
		expr = deref;
	} else {
		expr = expr_create(expr_prop_ref_t);
		expr->operation.prop_ref = ref;
	}

	return expr;
}

expression *expr_create(expression_type type)
{
	expression *expr = malloc(sizeof(expression));
	if (expr == NULL)
	{
		perror("Unable to allocate memory for expression");
		exit(1);
	}
	expr->type = type;

	return expr;
}

static void expr_destroy(expression *expr)
{
	switch (expr->type)
	{
		case expr_prop_ref_t:
			prop_ref_destroy(expr->operation.prop_ref);
			break;
		case expr_prop_set_t:
			prop_ref_destroy(expr->operation.prop_set->ref);
			expr_destroy(expr->operation.prop_set->value);
			free(expr->operation.prop_set);
			break;
		case expr_list_t:
			ll_destroy(expr->operation.expr_list, (destructor) expr_destroy);
			break;
		case expr_primitive_t:
			hval_release(expr->operation.primitive);
			break;
		default:
			hlog("ERROR: unexpected type passed to expr_destroy");
			break;
	}

	free(expr);
}

static void prop_ref_destroy(prop_ref *ref)
{
	if (ref->site)
	{
		expr_destroy(ref->site);
	}

	hstr_release(ref->name);
	free(ref);
}


expression *read_string(runtime *rt)
{
	token *t = runtime_current_token(rt);
	expression *expr = expr_create(expr_primitive_t);
	expr->operation.primitive = hval_string_create(t->value.string);
	return expr;
}

expression *read_number(runtime *rt)
{
	token *t = runtime_current_token(rt);
	expression *expr = expr_create(expr_primitive_t);
	expr->operation.primitive = hval_number_create(t->value.number);
	return expr;
}

static hval *runtime_evaluate_expression(runtime *rt, expression *expr, hval *context)
{
	switch (expr->type)
	{
		case expr_prop_ref_t:
			return eval_prop_ref(rt, expr->operation.prop_ref, context);
		case expr_prop_set_t:
			return eval_prop_set(rt, expr->operation.prop_set, context);
		case expr_list_t:
			return eval_expr_list(rt, expr->operation.expr_list, context);
		case expr_primitive_t:
			hval_retain(expr->operation.primitive);
			return expr->operation.primitive;
		default:
			hlog("Error: unknown expression type");
			exit(1);
	}
}

static hval *eval_expr_list(runtime *rt, linked_list *expr_list, hval *context)
{
	hval *result = NULL, *new_result = NULL;	
	ll_node *current = expr_list->head;
	expression *expr = NULL;
	while (current)
	{
		expr = (expression *) current->data;
		new_result = runtime_evaluate_expression(rt, expr, context);
		if (result != NULL)
		{
			hval_release(result);
		}

		result = new_result;
		current = current->next;
	}

	return result;
}

static hval *eval_prop_ref(runtime *rt, prop_ref *ref, hval *context)
{
	hval *site = get_prop_ref_site(rt, ref, context);
	if (site->type != hash_t)
	{
		hlog("eval_prop_ref expected a hash");
		exit(1);
	}

	hval *val = hval_hash_get(site, ref->name);
	/*hval_retain(val);*/
	return val;
}

static hval *eval_prop_set(runtime *rt, prop_set *set, hval *context)
{
	hval *site = get_prop_ref_site(rt, set->ref, context);
	if (site->type != hash_t)
	{
		hlog("eval_prop_set expected a hash");
		exit(1);
	}

	hval *value = runtime_evaluate_expression(rt, set->value, context);
	hval_hash_put(site, set->ref->name, value);
	/*hval_retain(value);*/
	return value;
}

static hval *get_prop_ref_site(runtime *rt, prop_ref *ref, hval *context)
{
	if (ref->site != NULL)
	{
		return runtime_evaluate_expression(rt, ref->site, context);
	}
	else
	{
		return context;
	}
}

hval *runtime_eval_loop(runtime *runtime)
{
	hval *context = hval_hash_create_child(runtime->top_level);
	token *tok = NULL;
	
	hval *last_result = NULL, *result = NULL;
	while (tok = runtime_get_next_token(runtime))
	{
		hlog("processing token: %p\n", tok);
		char *str = token_to_string(tok);
		hlog("evaluating token: %s\n", str);
		free(str);
		result = runtime_eval_token(tok, runtime, context, last_result);
		if (last_result)
		{
			/*hval_release(last_result);*/
		}

		last_result = result;
	}

	hlog("runtime_eval_loop terminating. context:\n");
	hash_dump(context->value.hash.members, (void *)hstr_to_str, (void *) hval_to_string);

	return context;
}

hval *runtime_eval_token(token *tok, runtime *runtime, hval *context, hval *last_result)
{
#ifdef DEBUG_EVAL_TOKEN
	char *s = token_to_string(tok);
	hlog("runtime_eval_token: %s\n", s);
	free(s);
#endif

	last_result = NULL;
	token *deref_name = NULL;
	hval *result = NULL;
	switch(tok->type)
	{
		case dereference:
			deref_name = runtime_get_next_token(runtime);
			result = runtime_eval_identifier(deref_name, runtime, runtime->last_result);
			break;
		case identifier:
			result = runtime_eval_identifier(tok, runtime, context);
			break;
		case hash_start:
			result = runtime_eval_hash(tok, runtime, context);
			break;
		case string:
			result = hval_string_create(tok->value.string);
			break;
		case number:
			result = hval_number_create(tok->value.number);
			break;
		case list_start:
			result = runtime_eval_list(runtime, context);
			break;
		default:
			hlog("unhandled token\n");
	}

	runtime->last_result = result;
	return result;
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
		hval *value = runtime_assignment(tok, runtime, context, h);
	}

	/*hash_dump(h->value.hash.members, (char * (*)(void *))hstr_to_str, (char * (*)(void *))hval_to_string);*/
	return h;
}

static hval *runtime_eval_list(runtime *runtime, hval *context)
{
	hval *list = hval_list_create();
	token *t = runtime_get_next_token(runtime);
	while (t->type != list_end)
	{
		hval *val = runtime_eval_token(t, runtime, context, NULL);
		hval_list_insert_tail(list, val);
		hval_release(val);
		val = NULL;
		t = runtime_get_next_token(runtime);
	}

	return list;
}

hval *runtime_eval_identifier(token *tok, runtime *runtime, hval *context)
{
	token *next_token = runtime_peek_token(runtime);
	if (next_token->type == assignment)
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
			hlog("no function %s\n", token_string(tok)->str);
			return NULL;
		}
		hval *result = runtime_apply_function(runtime, function, arguments);
		hval_destroy(arguments);
		arguments = NULL;

		return result;
	}
	else if (next_token->type == dereference)
	{
		hval *value = hval_hash_get(context, token_string(tok));
		runtime_get_next_token(runtime);
		next_token = runtime_get_next_token(runtime);
		runtime->last_result = value;
		return runtime_eval_identifier(next_token, runtime, value);
	}
	else
	{
		return hval_hash_get(context, tok->value.string);
	}
}

hval *runtime_assignment(token *name_token, runtime *runtime, hval *context, hval *target)
{
	// consume the assignment
	runtime_get_next_token(runtime);
	token *next_token = runtime_get_next_token(runtime);
	hval *value = runtime_eval_token(next_token, runtime, context, NULL);
	hval_hash_put(target, token_string(name_token), value);
	return value;
}

static hval *runtime_apply_function(runtime *runtime, hval *function, hval *arguments)
{
	if (function->type != native_function_t)
	{
		hlog("unexpected type");
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
		hlog("Error: unexpected %s (expected %s)",
			token_type_string_token(t),
			token_type_string(token_type));
	}
}

static hval *native_print(hval *this, hval *args)
{
	char *str = hval_to_string(args);
	puts(str);
	free(str);
	return NULL;
}

