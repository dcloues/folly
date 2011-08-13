#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "runtime.h"
#include "fmt.h"
#include "lexer.h"
#include "lexer_io.h"
#include "linked_list.h"
#include "log.h"
#include "type.h"
#include "ht.h"
#include "str.h"

/*typedef struct _loaded_file {*/
	/*expression *ast;*/
/*}*/

expression *runtime_analyze(runtime *);
token *runtime_peek_token(runtime *runtime);
token *runtime_get_next_token(runtime *runtime);
static void expect_token(token *t, type token_type);
static void register_top_level(runtime *);
static void register_builtin(runtime *, hval *, char *, hval *, bool);
static void register_builtin_r(runtime *, hval *, char *, hval *);

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
static hval *undefer(runtime *rt, hval *maybe_deferred);

static hval *get_prop_ref_site(runtime *, prop_ref *, hval *);

#define NATIVE_FUNCTION(name) hval *name(runtime *rt, hval *this, hval *args)

void runtime_extract_arg_list(runtime *rt, hval *args, ...);

static hval *native_print(runtime *, hval *this, hval *args);
static hval *native_add(runtime *, hval *this, hval *args);
static hval *native_subtract(runtime *, hval *this, hval *args);
static hval *native_fn(runtime *, hval *this, hval *args);
static hval *native_clone(runtime *, hval *this, hval *args);
static hval *native_extend(runtime *, hval *this, hval *args);
static hval *native_to_string(runtime *, hval *this, hval *args);
static hval *native_string_to_string(runtime *rt, hval *this, hval *args);
static hval *native_number_to_string(runtime *rt, hval *this, hval *args);
static hval *native_cond(runtime *rt, hval *this, hval *args);

static void init_booleans(runtime *rt);

NATIVE_FUNCTION(native_equals);
NATIVE_FUNCTION(native_while);
static NATIVE_FUNCTION(native_is_true);
static NATIVE_FUNCTION(native_lt);
static NATIVE_FUNCTION(native_gt);
static NATIVE_FUNCTION(native_or);
static NATIVE_FUNCTION(native_and);
static NATIVE_FUNCTION(native_not);
static NATIVE_FUNCTION(native_xor);
static NATIVE_FUNCTION(native_load);

static NATIVE_FUNCTION(native_show_heap);

#define runtime_current_token(rt) (rt->current)
#define runtime_error(...) fprintf(stderr, __VA_ARGS__); exit(1);
typedef void (*top_level_initializer)(runtime *);

static native_function_spec native_functions[] = {
	{ "Object.extend", (native_function) native_extend },
	{ "Object.clone", (native_function) native_clone },
	{ "Object.is_true", (native_function) native_is_true },
	{ "Object.to_string", (native_function) native_to_string },
	{ "String.to_string", (native_function) native_string_to_string },
	{ "Number.to_string", (native_function) native_number_to_string },
	{ "sys.load", (native_function) native_load },
	{ "sys.show_heap_info", (native_function) native_show_heap },
	{ "io.print", (native_function) native_print },
	{ "+", (native_function) native_add },
	{ "-", (native_function) native_subtract },
	{ "=", (native_function) native_equals },
	{ "<", (native_function) native_lt },
	{ ">", (native_function) native_gt },
	{ "fn", (native_function) native_fn },
	{ "cond", (native_function) native_cond },
	{ "while", (native_function) native_while },
	{ "or", (native_function) native_or },
	{ "and", (native_function) native_and },
	{ "not", (native_function) native_not },
	{ "xor", (native_function) native_xor },
	{ NULL, NULL }
};

static top_level_initializer top_level_initializers[] = {
	init_booleans,
	NULL
};

void runtime_init_globals()
{
	type_init_globals();
}

void runtime_destroy_globals()
{
	type_destroy_globals();
}

runtime *runtime_create()
{
	runtime *r = malloc(sizeof(runtime));
	r->current = NULL;
	r->mem = mem_create();
	r->peek = NULL;
	r->current = NULL;
	r->loaded_modules = NULL;

	r->object_root = NULL;
	r->object_root = hval_hash_create(r);
	mem_add_gc_root(r->mem, r->object_root);

	r->top_level = hval_hash_create(r);
	hstr *str = hstr_create("Object");
	hval_hash_put(r->top_level, str, r->object_root, r->mem);
	hstr_release(str);
	mem_add_gc_root(r->mem, r->top_level);

	// create a separate gc root for primitives creating while parsing
	// input. these primitives don't start with any other references,
	// so this is necessary to keep them from being collected.
	r->primitive_pool = hval_list_create(r);
	mem_add_gc_root(r->mem, r->primitive_pool);
	/*printf("primitive pool: %p\n", r->primitive_pool);*/

	//hlog("top_level: %p\n", r->top_level);
	register_top_level(r);
	return r;
}

void runtime_destroy(runtime *r)
{
	if (!r) return;

	if (r->loaded_modules) {
		ll_node *module_node = r->loaded_modules->head;
		while (module_node) {
			expr_destroy((expression *) module_node->data, true, r->mem);
			module_node = module_node->next;
		}

		ll_destroy(r->loaded_modules, NULL, NULL);
	}

	hlog("releasing top_level: %p\n", r->top_level);
	mem_remove_gc_root(r->mem, r->top_level);
	r->top_level = NULL;

	mem_remove_gc_root(r->mem, r->object_root);
	r->object_root = NULL;

	mem_remove_gc_root(r->mem, r->primitive_pool);
	r->primitive_pool = NULL;

	gc(r->mem);
	mem_destroy(r->mem);
	r->mem = NULL;
	hlog("done releasing top_level\n");
	free(r);
}

static void register_top_level(runtime *r)
{
	int i = 0;
	top_level_initializer *init = top_level_initializers;
	while (*init) {
		(*init)(r);
		init++;
	}

	hlog("Registering top levels under %p\n", r->top_level);
	for (i = 0; i < sizeof(native_functions); i++) {
		if (native_functions[i].path == NULL) {
			break;
		}
		hlog("registering: %s\n", native_functions[i].path);
		register_builtin_r(r,
			r->top_level,
			native_functions[i].path,
			hval_native_function_create(native_functions[i].function, r));
	}
}

static void init_booleans(runtime *rt) {
	register_builtin_r(rt, rt->top_level, "Boolean", hval_hash_create(rt));
	register_builtin_r(rt, rt->top_level, "true", hval_boolean_create(true, rt));
	register_builtin_r(rt, rt->top_level, "false", hval_boolean_create(false, rt));
}

static void register_builtin_r(runtime *rt, hval *site, char *name, hval *value)
{
	register_builtin(rt, site, name, value, true);
}

static void register_builtin(runtime *rt, hval *site, char *name, hval *value, bool release_value)
{
	int start = 0;
	int i = 0;
	hstr *str = NULL;
	hval *new_site = NULL;

	// for symmetry with the hval_release() below
	// we create missing values and need to release them,
	// so it's easier to retain the site we received
	// so we can release unconditionally
	hval_retain(site);
	while (true)
	{
		if (name[i] == '.') {
			str = hstr_create_len(name + start, i - start);
			new_site = hval_hash_get(site, str, rt);
			if (new_site == NULL) {
				new_site = hval_hash_create(rt);
				hval_hash_put(site, str, new_site, rt->mem);
			}
			hval_release(site, rt->mem);
			site = new_site;
			hstr_release(str);
			start = i + 1;
		} else if (name[i] == '\0') {
			str = hstr_create_len(name + start, i - start);
			break;
		}

		++i;
	}

	hlog("registering %p at %p\n", value, site);
	hval_hash_put(site, str, value, rt->mem);
	if (hval_is_callable(value)) {
		hval_bind_function(value, site, rt->mem);
	}

	hstr_release(str);
	if (release_value) {
		hval_release(value, rt->mem);
	}
	hval_release(site, rt->mem);
}

hval *runtime_exec_one(runtime *runtime, lexer_input *input, bool *terminated)
{
	runtime->input = input;
	token *t = runtime_get_next_token(runtime);
	if (t == NULL) {
		*terminated = true;
		return NULL;
	}

	hval *result = NULL;
	expression *expr = read_complete_expression(runtime);
	if (expr != NULL) {
		result = runtime_evaluate_expression(runtime, expr, runtime->top_level);
		expr_destroy(expr, false, runtime->mem);
	} else {
		*terminated = true;
	}

	return result;
}

hval *runtime_exec(runtime *runtime, lexer_input *input)
{
	hlog("analyzing...\n");
	runtime->input = input;
	expression *expr = runtime_analyze(runtime);

	hlog("creating main context\n");
	mem_add_gc_root(runtime->mem, runtime->top_level);

	hlog("beginning evaluation\n");
	hval *ret = runtime_evaluate_expression(runtime, expr, runtime->top_level);
	hlog("runtime_eval complete - got return value %p\n", ret);

	expr_destroy(expr, true, runtime->mem);
	expr = NULL;

	char *str = hval_to_string(ret);
	hlog("eval got result: %s", str);
	free(str);

	return runtime->top_level;
}

hval *runtime_load_module(runtime *runtime, lexer_input *input)
{
	lexer_input *orig_input = runtime->input;
	runtime->input = input;
	expression *expr = runtime_analyze(runtime);
	if (runtime->loaded_modules == NULL) {
		runtime->loaded_modules = ll_create();
	}

	ll_insert_head(runtime->loaded_modules, expr);

	hval *ret = runtime_evaluate_expression(runtime, expr, runtime->top_level);
	runtime->input = orig_input;
	return ret;
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
			runtime_error("read_complete_expression returned null\n");
			exit(1);
		}
		ll_insert_tail(expr_list->operation.expr_list, expr);
	}

	return expr_list;
}

expression *read_complete_expression(runtime *rt)
{
	expression *expr = NULL;
	token_type tt = rt->current->type;

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
		case list_start:
			expr = read_list(rt);
			break;
		case quote:
			expr = read_quoted(rt);
			break;
		default:
			runtime_error("unhandled token type: %s\n", token_type_string(tt));
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

	token *t = rt->current;
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
	expr->operation.primitive = hval_string_create(t->value.string, rt);
	hval_list_insert_head(rt->primitive_pool, expr->operation.primitive);
	return expr;
}

expression *read_number(runtime *rt)
{
	token *t = runtime_current_token(rt);
	expression *expr = expr_create(expr_primitive_t);
	expr->operation.primitive = hval_number_create(t->value.number, rt);
	hval_list_insert_head(rt->primitive_pool, expr->operation.primitive);
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
		/*if (result != NULL)*/
		/*{*/
			/*hval_release(result, rt->mem);*/
		/*}*/

		result = new_result;
		current = current->next;
	}

	return result;
}

static hval *eval_expr_list_literal(runtime *rt, expression *expr_list, hval *context)
{
	hlog("eval_expr_list_literal\n");
	hval *list = hval_list_create(rt);
	mem_add_gc_root(rt->mem, list);
	ll_node *current = expr_list->operation.list_literal->head;
	expression *expr = NULL;
	hval *result = NULL;
	while (current)
	{
		expr = (expression *) current->data;
		result = runtime_evaluate_expression(rt, expr, context);
		hval_list_insert_tail(list, result);
		hval_release(result, rt->mem);
		result = NULL;
		current = current->next;
	}

	mem_remove_gc_root(rt->mem, list);

	return list;
}

static hval *eval_expr_hash_literal(runtime *rt, hash *def, hval *context)
{
	hval *result = hval_hash_create(rt);
	mem_add_gc_root(rt->mem, result);
	hash_iterator *iter = hash_iterator_create(def);
	while (iter->current_key != NULL)
	{
		hval *expr_value = runtime_evaluate_expression(rt, iter->current_value, context);
		hval_hash_put(result, iter->current_key, expr_value, rt->mem);
		hval_release(expr_value, rt->mem);
		hash_iterator_next(iter);
	}

	hash_iterator_destroy(iter);
	mem_remove_gc_root(rt->mem, result);
	return result;
}

static hval *eval_prop_ref(runtime *rt, prop_ref *ref, hval *context)
{
	hval *site = get_prop_ref_site(rt, ref, context);
	hval *val = hval_hash_get(site, ref->name, rt);
	if (val != NULL) {
		hval_retain(val);
	} else {
		runtime_error("warning: attempted to access undefined property %s of %p\n", ref->name->str, site);
	}

	if (site != context)
	{
		hval_release(site, rt->mem);
	}
	return val;
}

static hval *eval_prop_set(runtime *rt, prop_set *set, hval *context)
{
	hval *site = get_prop_ref_site(rt, set->ref, context);
	mem_add_gc_root(rt->mem, site);
	if (site->type != hash_t)
	{
		hlog("eval_prop_set expected a hash");
		exit(1);
	}

	hval *value = runtime_evaluate_expression(rt, set->value, context);
	hval_hash_put(site, set->ref->name, value, rt->mem);
	if (site != context)
	{
		hval_release(site, rt->mem);
	}

	mem_remove_gc_root(rt->mem, site);
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

	return runtime_call_function(rt, fn, args, context);
}

hval *runtime_call_function(runtime *rt, hval *fn, hval *args, hval *context)
{
	static int gc_count = 0;

	mem_add_gc_root(rt->mem, args);
	hval *result = NULL;
	if (fn->type == native_function_t) {
		hval *self = hval_get_self(fn);
		result = fn->value.native_fn(rt, self, args);
	} else {
		result = eval_expr_folly_invocation(rt, fn, args, context);
	}

	mem_remove_gc_root(rt->mem, args);
	if (++gc_count == 1000) {
		gc_count = 0;
		gc_with_temp_root(rt->mem, result);
	}
	return result;
}

hval *runtime_call_hnamed_function(runtime *rt, hstr *name, hval *site, hval *args, hval *context) {
	hval *func = hval_hash_get(site, name, rt);
	return runtime_call_function(rt, func, args, context);
}

static hval *eval_expr_folly_invocation(runtime *rt, hval *fn, hval *args, hval *context)
{
	hval *expr = hval_hash_get(fn, FN_EXPR, rt);
	hval *default_args = hval_hash_get(fn, FN_ARGS, rt);
	if (default_args->type != args->type) {
		hlog("Error: argument type mismatch\n");
	}

	hval *fn_context = hval_hash_create_child(expr->value.deferred_expression.ctx, rt);
	/*hlog("created function context %p with parent %p\n", fn_context, expr->value.deferred_expression.ctx);*/
	mem_add_gc_root(rt->mem, fn_context);
	if (args->type == hash_t) {
		/*hlog("put all: default args\n");*/
		hval_hash_put_all(fn_context, default_args, rt->mem);
		/*hlog("put all: args\n");*/
		hval_hash_put_all(fn_context, args, rt->mem);
		/*hlog("get self..\n");*/
		hval *self = hval_get_self(fn);
		/*hlog("setting self: %p\n", self);*/
		hval_hash_put(fn_context, FN_SELF, self, rt->mem);
	}

	hval *result = eval_expr_list(rt, expr->value.deferred_expression.expr->operation.list_literal, fn_context);
	mem_remove_gc_root(rt->mem, fn_context);

	return result;
}

static hval *eval_expr_deferred(runtime *rt, expression *deferred, hval *context)
{
	hval *val = hval_create(deferred_expression_t, rt);
	val->value.deferred_expression.expr = deferred;
	expr_retain(deferred);
	val->value.deferred_expression.ctx = context;
	return val;
}

token *runtime_peek_token(runtime *runtime)
{
	if (!runtime->peek) {
		runtime->peek = get_next_token(runtime->input);
	}

	return runtime->peek;
}

token *runtime_get_next_token(runtime *runtime)
{
	if (runtime->current) {
		token_destroy(runtime->current, NULL);
	}

	token *t = NULL;
	if (runtime->peek) {
		t = runtime->peek;
		runtime->peek = NULL;
	} else {
		t = get_next_token(runtime->input);
	}

	runtime->current = t;
	return t;
}

static void expect_token(token *t, type token_type)
{
	if (!t || token_type != t->type)
	{
		hlog("Error: unexpected %s (expected %s)",
			token_type_string_token(t),
			token_type_string(token_type));
	}
}

static hval *native_print(runtime *rt, hval *self, hval *args)
{
	hstr *name = hstr_create("to_string");
	hval *str = NULL;
	if (args->type == hash_t) {
		printf("can't print a hash yet");
	} else {
		ll_node *node = args->value.list->head;
		hval *arg = NULL;
		bool printed_any = false;
		while (node) {
			if (printed_any) {
				printf(" ");
			}
			printed_any = true;

			arg = (hval *) node->data;
			str = runtime_call_hnamed_function(rt, name, arg, NULL, rt->top_level);
			fputs(str->value.str->str, stdout);
			hval_release(str, rt->mem);
			str = NULL;
			node = node->next;
		}
		if (printed_any) {
			fputc('\n', stdout);
		}
	}

	hstr_release(name);
	name = NULL;
	return NULL;
}

static hval *native_add(runtime *rt, hval *this, hval *args)
{
	int sum = 0;
	ll_node *current = args->value.list->head;
	while (current) {
		sum += ((hval *) current->data)->value.number;
		current = current->next;
	}

	return hval_number_create(sum, rt);
}

static hval *native_subtract(runtime *rt, hval *this, hval *args)
{
	int val = 0;
	linked_list *arglist = args->value.list;
	if (arglist->size == 0) {
		val = 0;
	} else if (arglist->size == 1) {
		val = -((hval *) arglist->head->data)->value.number;
	} else {
		val = ((hval *) arglist->head->data)->value.number;
		ll_node *current = arglist->head->next;
		while (current) {
			val = val - ((hval *) current->data)->value.number;
			current = current->next;
		}
	}

	return hval_number_create(val, rt);
}

NATIVE_FUNCTION(native_equals) {
	if (args->type != list_t || args->value.list->size < 2) {
		runtime_error("native_equals: arguments must be a list with at least 2 elements\n");
	}

	// TODO Make this polymorphic, using an = method on objects
	hval *ref = (hval *) args->value.list->head->data;
	hval *candidate = NULL;
	bool equals = true;
	ll_node *node = args->value.list->head->next;
	while (node && equals) {
		candidate = (hval *) node->data;
		if (ref->type != candidate->type) {
			/*runtime_error("type mismatch in native_equals\n");*/
			equals = false;
			break;
		}

		switch (ref->type) {
		case number_t:
			equals = ref->value.number == candidate->value.number;
			break;
		case string_t:
			equals = strcmp(ref->value.str->str, candidate->value.str->str) == 0;
			break;
		default:
			equals = ref == candidate;
			break;
		}

		node = node->next;
	}

	return hval_number_create(equals ? 1 : 0, rt);
}

static NATIVE_FUNCTION(native_is_true)
{
	return hval_hash_get(rt->top_level, hval_is_true(this) ? TRUE : FALSE, rt);
}

static NATIVE_FUNCTION(native_lt)
{
	hval *arg1 = NULL;
	hval *arg2 = NULL;
	runtime_extract_arg_list(rt, args, &arg1, number_t, &arg2, number_t, NULL);
	
	bool lt = arg1->value.number < arg2->value.number;
	return hval_number_create(lt ? 1 : 0, rt);
}

static NATIVE_FUNCTION(native_gt)
{
	hval *arg1 = NULL;
	hval *arg2 = NULL;
	runtime_extract_arg_list(rt, args, &arg1, number_t, &arg2, number_t, NULL);
	bool gt = arg1->value.number > arg2->value.number;
	return hval_number_create(gt ? 1 : 0, rt);
}

static hval *native_fn(runtime *rt, hval *this, hval *args)
{
	hval *fn = hval_hash_create(rt);

	hval_hash_put(fn, FN_ARGS, (hval *) args->value.list->head->data, rt->mem);
	hval_hash_put(fn, FN_EXPR, (hval *) args->value.list->tail->data, rt->mem);
	return fn;
}

static hval *native_clone(runtime *rt, hval *this, hval *args)
{
	hlog("native_clone: %p\n", this);
	return hval_clone(this, rt);
}

static hval *native_extend(runtime *rt, hval *this, hval *args)
{
	hlog("native_extend: %p\n", this);
	hval *sub = hval_hash_create_child(this, rt);
	hash_iterator *iter = hash_iterator_create(args->members);
	while (iter->current_key) {
		if (iter->current_key != PARENT) {
			hval_hash_put(sub, iter->current_key, iter->current_value, rt->mem);
		}
		hash_iterator_next(iter);
	}

	hash_iterator_destroy(iter);
	return sub;

}

static hval *native_to_string(runtime *rt, hval *this, hval *args) {
	char *str = hval_to_string(this);
	hstr *hs = hstr_create(str);
	free(str);
	str = NULL;

	hval *obj = hval_string_create(hs, rt);
	hstr_release(hs);
	return obj;
}

static hval *native_string_to_string(runtime *rt, hval *this, hval *args) {
	return this;
}

static hval *native_number_to_string(runtime *rt, hval *this, hval *args) {
	char *str = fmt("%d", this->value.number);
	hstr *hs = hstr_create(str);
	free(str);
	str = NULL;
	hval *obj = hval_string_create(hs, rt);
	hstr_release(hs);
	return obj;
}

static hval *native_cond(runtime *rt, hval *this, hval *args)
{
	if (args->type != list_t) {
		runtime_error("native_cond: argument mismatch: expected list");
	}

	ll_node *test_node = args->value.list->head;
	while (test_node) {
		hval *cond_hval = (hval *) test_node->data;
		linked_list *cond_pair = cond_hval->value.list;

		hval *test = undefer(rt, (hval *) cond_pair->head->data);

		if (hval_is_true(test)) {
			return cond_pair->head != cond_pair->tail ? undefer(rt, (hval *) cond_pair->tail->data) : test;
			break;
		}

		test_node = test_node->next;
	}
	
	return NULL;
}

NATIVE_FUNCTION(native_while)
{
	hval *test = NULL;
	hval *body = NULL;
	runtime_extract_arg_list(rt, args, &test, deferred_expression_t, &body, deferred_expression_t, NULL);

	hval *result = NULL;
	while (hval_is_true(undefer(rt, test))) {
		result = undefer(rt, body);
		/*hval *cond = undefer(test);*/
	}

	return result;
}

static hval *undefer(runtime *rt, hval *maybe_deferred) {
	mem_add_gc_root(rt->mem, maybe_deferred);
	if (maybe_deferred->type == deferred_expression_t) {
		deferred_expression *def = &(maybe_deferred->value.deferred_expression);
		hval *result = eval_expr_list(rt, def->expr->operation.list_literal, def->ctx);
		mem_remove_gc_root(rt->mem, maybe_deferred);
		return result;
	}

	mem_remove_gc_root(rt->mem, maybe_deferred);
	return maybe_deferred;
}

void runtime_extract_arg_list(runtime *rt, hval *arglist, ...)
{
	if (arglist->type != list_t) {
		runtime_error("argument error: expected list");
	}

	va_list vargs;
	va_start(vargs, arglist);
	hval **dest = NULL;
	hval *value = NULL;
	type expected_type;
	ll_node *arglist_node = arglist->value.list->head;
	while ((dest = va_arg(vargs, hval**)) != NULL) {
		expected_type = va_arg(vargs, type);
		value = (hval *) arglist_node->data;
		if (value->type == expected_type) {
			*dest = value;
		} else {
			runtime_error("argument error: got %s, expected %s\n", hval_type_string(expected_type), hval_type_string(value->type));
		}
		arglist_node = arglist_node->next;
	}

	va_end(vargs);
}

NATIVE_FUNCTION(native_and)
{
	if (args->type != list_t) {
		runtime_error("argument mismatch: expected list");
	}

	ll_node *node = args->value.list->head;
	bool result = true;
	while (node && result) {
		hval *current = (hval *) node->data;
		current = undefer(rt, current);
		if (!hval_is_true(current)) {
			result = false;
		}
		node = node->next;
	}

	return hval_number_create(result ? 1 : 0, rt);
}

NATIVE_FUNCTION(native_or)
{
	if (args->type != list_t) {
		runtime_error("argument mismatch: expected list");
	}

	ll_node *node = args->value.list->head;
	bool result = false;
	while (node && !result) {
		hval *current = (hval *) node->data;
		current = undefer(rt, current);
		if (hval_is_true(current)) {
			result = true;
		}
		node = node->next;
	}

	return hval_number_create(result ? 1 : 0, rt);
}

NATIVE_FUNCTION(native_not)
{
	if (args->type != list_t) {
		runtime_error("argument mismatch: expected list");
	}

	if (args->value.list->size != 1) {
		runtime_error("argument count mismatch: not() accepts exactly 1");
	}

	hval *value = (hval *) args->value.list->head->data;
	return hval_number_create(hval_is_true(value) ? 0 : 1, rt);
}

NATIVE_FUNCTION(native_xor)
{
	if (args->type != list_t) {
		runtime_error("argument mismatch: expected list");
	}
	ll_node *node = args->value.list->head;
	int truths = 0;
	while (node) {
		hval *val = (hval *) node->data;
		if (hval_is_true(val)) {
			++truths;
			if (truths > 1) {
				break;
			}
		}

		node = node->next;
	}

	return hval_number_create(truths == 1 ? 1 : 0, rt);
}

NATIVE_FUNCTION(native_load)
{
	hval *file = NULL;
	hval *context = rt->top_level;
	/*if (args->type == list_t) {*/
	runtime_extract_arg_list(rt, args, &file, string_t, NULL);
	/*} else {*/
		/*static hstr *key_filename, *key_into;*/
		/*if (key_filename == NULL) key_filename = hstr_create("file");*/
		/*if (key_into == NULL) key_into = hstr_create("into");*/

		/*file = hval_hash_get(args, key_filename, rt);*/
		/*context = hval_hash_get(args, key_into, rt);*/
	/*}*/

	char *filename = file->value.str->str;
	struct stat stat_buf;
	int err = stat(filename, &stat_buf);
	if (err == -1) {
		perror("Unable to open file");
		return hval_hash_get(rt->top_level, FALSE, rt);
	} else if (!S_ISREG(stat_buf.st_mode)) {
		return hval_hash_get(rt->top_level, FALSE, rt);
	}

	lexer_input *input = lexer_file_input_create(filename);
	runtime_load_module(rt, input);
	lexer_input_destroy(input);
	return hval_hash_get(rt->top_level, TRUE, rt);
}

NATIVE_FUNCTION(native_show_heap)
{
	debug_heap_output(rt->mem);
	return NULL;
	/*printf("heap size: %s bytes in %d chunks\n", rt->mem->*/
}

