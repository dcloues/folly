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

void runtime_parse(runtime *runtime, char *file);
expression *runtime_analyze(runtime *);
token *runtime_peek_token(runtime *runtime);
token *runtime_get_next_token(runtime *runtime);
static void *expect_token(token *t, type token_type);
static void register_top_level(runtime *);

static expression *expr_create(expression_type);
static void expr_destroy(expression *expr);
static void prop_ref_destroy(prop_ref *ref);
static expression *read_complete_expression(runtime *);
static expression *read_identifier(runtime *);
static expression *read_number(runtime *);
static expression *read_string(runtime *);
static expression *read_list(runtime *);

static hval *runtime_evaluate_expression(runtime *, expression *, hval *);
static hval *eval_prop_ref(runtime *, prop_ref *, hval *);
static hval *eval_prop_set(runtime *, prop_set *, hval *);
static hval *eval_expr_list(runtime *, linked_list *, hval *);
static hval *eval_expr_list_literal(runtime *, expression *, hval *);
static hval *eval_expr_invocation(runtime *, invocation *, hval *);

static hval *get_prop_ref_site(runtime *, prop_ref *, hval *);

static hval *native_print(hval *this, hval *args);
static hval *native_add(hval *this, hval *args);
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

		hlog("releasing top_level\n");
		hval_release(r->top_level);
		r->top_level = NULL;
		hlog("done releasing top_level\n");
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

	hstr *plus = hstr_create("+");
	hval *add = hval_native_function_create(native_add);
	hval_hash_put(r->top_level, plus, add);
	hstr_release(plus);
	hval_release(add);
}

hval *runtime_eval(runtime *runtime, char *file)
{
	runtime_parse(runtime, file);
	// this will have type=expr_list_t
	hlog("analyzing...\n");
	expression *expr = runtime_analyze(runtime);
	hlog("creating main context\n");
	hval *context = hval_hash_create_child(runtime->top_level);
	hlog("beginning evaluation\n");
	hval *ret = runtime_evaluate_expression(runtime, expr, context);
	hlog("runtime_eval complete - got return value %p\n", ret);
	expr_destroy(expr);
	expr = NULL;
	char *str = hval_to_string(ret);
	hlog("eval got result: %s", str);
	free(str);
	if (ret != NULL)
	{
		hval_release(ret);
	}

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
	hlog("read_identifier: %s\n", t->value.string->str);
	ref->site = NULL;
	hstr_retain(t->value.string);

	token *next = runtime_peek_token(rt);
	if (next->type == assignment) {
		hlog("reading assignment\n");
		// consume the assignment and advance to the next
		runtime_get_next_token(rt);
		runtime_get_next_token(rt);
		expression *assgn = expr_create(expr_prop_set_t);
		assgn->operation.prop_set = malloc(sizeof(prop_set));
		assgn->operation.prop_set->ref = ref;
		assgn->operation.prop_set->value = read_complete_expression(rt);
		expr = assgn;
	} else if (next->type == dereference) {
		hlog("reading dereference\n");
		// consume the assignment and advance to the next
		runtime_get_next_token(rt);
		runtime_get_next_token(rt);
		expr = read_complete_expression(rt);
		expression *parent = expr_create(expr_prop_ref_t);
		parent->operation.prop_ref = ref;
		if (expr->type == expr_invocation_t)
		{
			expr->operation.invocation->function->operation.prop_ref->site = parent;
		}
		else if (expr->type == expr_prop_ref_t)
		{
			expr->operation.prop_ref->site = parent;
		}
	} else if (next->type == list_start) {
		runtime_get_next_token(rt);
		expr = expr_create(expr_invocation_t);
		invocation *inv = malloc(sizeof(invocation));
		if (inv == NULL)
		{
			perror("Unable to allocate memory for invocation\n");
			exit(1);
		}
		
		expression *func = expr_create(expr_prop_ref_t);
		func->operation.prop_ref = ref;
		inv->function = func;
		inv->list_args = read_list(rt);
		expr->operation.invocation = inv;
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
	hlog("expr_destroy %p %d\n", expr, expr->type);
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
		case expr_list_literal_t:
			ll_destroy(expr->operation.list_literal, (destructor) expr_destroy);
			break;
		case expr_list_t:
			ll_destroy(expr->operation.expr_list, (destructor) expr_destroy);
			break;
		case expr_primitive_t:
			hval_release(expr->operation.primitive);
			break;
		case expr_invocation_t:
			expr_destroy(expr->operation.invocation->list_args);
			expr_destroy(expr->operation.invocation->function);
			free(expr->operation.invocation);
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

expression *read_list(runtime *rt)
{
	expression *list = malloc(sizeof(expression));
	list->type = expr_list_literal_t;
	if (list == NULL)
	{
		perror("Unable to allocate memory in read_list\n");
		exit(1);
	}
	list->operation.list_literal = ll_create();

	runtime_get_next_token(rt);
	token *t = runtime_current_token(rt);
	expression *expr = NULL;
	while (t != NULL && t->type != list_end)
	{
		expr = read_complete_expression(rt);
		ll_insert_tail(list->operation.list_literal, expr);
		t = runtime_get_next_token(rt);
	}

	return list;
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
		case expr_list_literal_t:
			return eval_expr_list_literal(rt, expr, context);
		case expr_primitive_t:
			hval_retain(expr->operation.primitive);
			return expr->operation.primitive;
		case expr_invocation_t:
			return eval_expr_invocation(rt, expr->operation.invocation, context);
		default:
			hlog("Error: unknown expression type");
			exit(1);
	}
}

static hval *eval_expr_list(runtime *rt, linked_list *expr_list, hval *context)
{
	hlog("eval_expr_list\n");
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

static hval *eval_expr_list_literal(runtime *rt, expression *expr_list, hval *context)
{
	hlog("eval_expr_list_literal\n");
	hval *list = hval_list_create();
	ll_node *current = expr_list->operation.list_literal->head;
	expression *expr = NULL;
	hval *result = NULL;
	while (current)
	{
		expr = (expression *) current->data;
		result = runtime_evaluate_expression(rt, expr, context);
		hval_list_insert_tail(list, result);
		hval_release(result);
		result = NULL;
		current = current->next;
	}

	return list;
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
	hval_retain(val);
	if (site != context)
	{
		hval_release(site);
	}
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

static hval *eval_expr_invocation(runtime *rt, invocation *inv, hval *context)
{
	hlog("eval_expr_invocation\n");
	hval *fn = runtime_evaluate_expression(rt, inv->function, context);
	if (fn == NULL)
	{
		return NULL;
	}
	hval *args = eval_expr_list_literal(rt, inv->list_args, context);
	hval *result = fn->value.native_fn(NULL, args);
	hval_release(args);
	hval_release(fn);
	hlog("eval_expr_invocation got result: %p\n", result);

	return result;
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
	hlog("native_print: %s\n", str);
	puts(str);
	free(str);
	return NULL;
}

static hval *native_add(hval *this, hval *args)
{
	int sum = 0;
	ll_node *current = args->value.list->head;
	while (current) {
		sum += ((hval *) current->data)->value.number;
		current = current->next;
	}

	return hval_number_create(sum);
}
