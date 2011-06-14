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

static expression *read_complete_expression(runtime *);
static expression *read_identifier(runtime *);
static expression *read_number(runtime *);
static expression *read_string(runtime *);
static expression *read_list(runtime *);
static expression *read_hash(runtime *);
static expression *read_quoted(runtime *);

static hval *runtime_evaluate_expression(runtime *, expression *, hval *);
static hval *eval_prop_ref(runtime *, prop_ref *, hval *);
static hval *eval_prop_set(runtime *, prop_set *, hval *);
static hval *eval_expr_hash_literal(runtime *, hash *, hval *);
static hval *eval_expr_list(runtime *, linked_list *, hval *);
static hval *eval_expr_list_literal(runtime *, expression *, hval *);
static hval *eval_expr_invocation(runtime *, invocation *, hval *);
static hval *eval_expr_folly_invocation(runtime *rt, hval *fn, hval *args, hval *context);
static hval *eval_expr_deferred(runtime *, expression *, hval *);

static hval *get_prop_ref_site(runtime *, prop_ref *, hval *);

static hval *native_print(hval *this, hval *args);
static hval *native_add(hval *this, hval *args);
static hval *native_fn(hval *this, hval *args);
#define runtime_current_token(rt) ((token *) rt->current->data)

typedef struct {
	char *name;
	native_function fn;
} native_function_declaration;

static native_function_declaration top_levels[] = {
	{"print", native_print},
	{NULL, NULL}
};

static hstr *FN_ARGS;
static hstr *FN_EXPR;

void runtime_init_globals()
{

	FN_ARGS = hstr_create("__arguments__");
	FN_EXPR = hstr_create("__expr__");
}

void runtime_destroy_globals()
{
	hstr_release(FN_ARGS);
	hstr_release(FN_EXPR);
}

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

	hval *fn = hval_native_function_create(native_fn);
	str = hstr_create("fn");
	hval_hash_put(r->top_level, str, fn);
	hstr_release(str);
	hval_release(fn);
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
	expression *expr_list = expr_create(expr_list_t);
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
		case hash_start:
			expr = read_hash(rt);
			break;
		case quote:
			expr = read_quoted(rt);
			break;
	}

	return expr;
}

expression *read_quoted(runtime *rt)
{
	
	expression *expr = expr_create(expr_deferred_t);
	runtime_get_next_token(rt);
	expression *deferred = read_complete_expression(rt);
	expr->operation.deferred_expression = deferred;
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
		else if (expr->type == expr_prop_set_t)
		{
			expr->operation.prop_set->ref->site = parent;
		}
	} else if (next->type == list_start || next->type == hash_start) {
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
		if (next->type == list_start)
		{
			inv->list_args = read_list(rt);
			inv->hash_args = NULL;
		}
		else
		{
			inv->list_args = NULL;
			inv->hash_args = read_hash(rt);
		}
		expr->operation.invocation = inv;
	} else {
		expr = expr_create(expr_prop_ref_t);
		expr->operation.prop_ref = ref;
	}

	return expr;
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
	expression *list = expr_create(expr_list_literal_t);
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

static expression *read_hash(runtime *rt)
{
	expression *hash_lit = expr_create(expr_hash_literal_t);
	hash_lit->operation.hash_literal = hash_create((hash_function) hash_hstr, (key_comparator) hstr_comparator);
	// consume the hash_start
	runtime_get_next_token(rt);
	token *t = runtime_current_token(rt);
	while (t != NULL && t->type != hash_end)
	{
		expect_token(t, identifier);
		hstr *key = t->value.string;
		hstr_retain(key);
		t = runtime_get_next_token(rt);
		expect_token(t, assignment);
		runtime_get_next_token(rt);
		expression *value = read_complete_expression(rt);
		hash_put(hash_lit->operation.hash_literal, key, value);
		t = runtime_get_next_token(rt);
	}

	return hash_lit;
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
		case expr_hash_literal_t:
			return eval_expr_hash_literal(rt, expr->operation.hash_literal, context);
		case expr_primitive_t:
			hval_retain(expr->operation.primitive);
			return expr->operation.primitive;
		case expr_invocation_t:
			return eval_expr_invocation(rt, expr->operation.invocation, context);
		case expr_deferred_t:
			return eval_expr_deferred(rt, expr->operation.deferred_expression, context);
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

static hval *eval_expr_hash_literal(runtime *rt, hash *def, hval *context)
{
	hval *result = hval_hash_create();
	hash_iterator *iter = hash_iterator_create(def);
	while (iter->current_key != NULL)
	{
		hval *expr_value = runtime_evaluate_expression(rt, iter->current_value, context);
		hval_hash_put(result, iter->current_key, expr_value);
		hval_release(expr_value);
		hash_iterator_next(iter);
	}

	hash_iterator_destroy(iter);
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
	if (val == NULL)
	{
		char *str = hstr_to_str(ref->name);
		hlog("warning: attempted to access undefined property %s\n", str);
		free(str);
		exit(1);
	}
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
	if (site != context)
	{
		hval_release(site);
	}
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
	hval *args = (inv->list_args != NULL)
			? eval_expr_list_literal(rt, inv->list_args, context)
			: eval_expr_hash_literal(rt, inv->hash_args->operation.hash_literal, context);
	if (args == NULL)
	{
		printf("null args!\n");
		exit(1);
	}

	/*hval *args = eval_expr_list_literal(rt, inv->list_args, context);*/
	hval *result = NULL;
	if (fn->type == native_function_t)
	{
		result = fn->value.native_fn(NULL, args);
	}
	else
	{
		result = eval_expr_folly_invocation(rt, fn, args, context);
	}
	hval_release(args);
	hval_release(fn);
	hlog("eval_expr_invocation got result: %p\n", result);

	return result;
}
	
static hval *eval_expr_folly_invocation(runtime *rt, hval *fn, hval *args, hval *context)
{
	hval *expr = hval_hash_get(fn, FN_EXPR);
	hval *default_args = hval_hash_get(fn, FN_ARGS);
	if (default_args->type != args->type) {
		hlog("Error: argument type mismatch\n");
	}

	hval *fn_context = hval_hash_create_child(expr->value.deferred_expression.ctx);
	if (args->type == hash_t) {
		hval_hash_put_all(fn_context, default_args);
		hval_hash_put_all(fn_context, args);
	}

	/*hval *fn_context = hval_hash_create_child(expr->value.deferred_expression.ctx);*/
	hval *result = runtime_evaluate_expression(rt, expr->value.deferred_expression.expr, fn_context);
	hval_release(fn_context);

	return result;
}

static hval *eval_expr_deferred(runtime *rt, expression *deferred, hval *context)
{
	hval *val = hval_create(deferred_expression_t);
	val->value.deferred_expression.expr = deferred;
	expr_retain(deferred);
	val->value.deferred_expression.ctx = context;
	return val;
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

static hval *native_fn(hval *this, hval *args)
{
	hval *fn = hval_hash_create();

	hval_hash_put(fn, FN_ARGS, (hval *) args->value.list->head->data);
	hval_hash_put(fn, FN_EXPR, (hval *) args->value.list->tail->data);
	return fn;
}

