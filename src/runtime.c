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
#include "smalloc.h"
#include "str.h"
#include "modules/file.h"
#include "modules/list.h"
#include "modules/object.h"

expression *runtime_analyze(runtime *, lexer *);
typedef void (*module_initializer)(runtime *, native_function_spec **, int *);
static void expect_token(token *t, type token_type);
static void register_top_level(runtime *);
static void register_builtin(runtime *, hval *, char *, hval *, bool);
static void register_builtin_r(runtime *, hval *, char *, hval *);
static void init_module(runtime *rt, module_initializer init);

static expression *read_complete_expression(lexer *);
static expression *read_identifier(lexer *);
static expression *read_number(lexer *);
static expression *read_string(lexer *);
static expression *read_list(lexer *);
static expression *read_hash(lexer *);
static expression *read_quoted(lexer *);
static expression *read_function_declaration(lexer *rt, expression *args);

static hval *runtime_evaluate_expression(runtime *, expression *, hval *);
static hval *eval_prop_ref(runtime *, prop_ref *, hval *);
static hval *eval_prop_set(runtime *, prop_set *, hval *);
static hval *eval_expr_hash_literal(runtime *, hash *, hval *);
static hval *eval_expr_list(runtime *, linked_list *, hval *);
static hval *eval_expr_list_literal(runtime *, expression *, hval *);
static hval *eval_expr_function_declaration(runtime *, function_declaration *, hval *);
static hval *eval_expr_invocation(runtime *, invocation *, hval *);
static hval *eval_expr_folly_invocation(runtime *rt, hval *fn, hval *args, hval *context);
static list_hval *eval_expr_function_args(runtime *rt, expression *expr, bool for_invocation, hval *context);
static hval *eval_expr_deferred(runtime *, expression *, hval *);
static hval *undefer(runtime *rt, hval *maybe_deferred);

static hval *get_prop_ref_site(runtime *, prop_ref *, hval *);

static NATIVE_FUNCTION(native_print);
static NATIVE_FUNCTION(native_add);
static NATIVE_FUNCTION(native_subtract);
static NATIVE_FUNCTION(native_fn);
static NATIVE_FUNCTION(native_clone);
static NATIVE_FUNCTION(native_extend);
static NATIVE_FUNCTION(native_to_string);
static NATIVE_FUNCTION(native_string_to_string);
static NATIVE_FUNCTION(native_number_to_string);
static NATIVE_FUNCTION(native_cond);

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
static NATIVE_FUNCTION(native_string_concat);

static NATIVE_FUNCTION(native_show_heap);

#define lexer_current_token(rt) (rt->current)
typedef void (*top_level_initializer)(runtime *);

typedef struct module_spec {
	module_initializer init;
	void (*shutdown)(runtime *);
} module_spec;

static module_spec default_modules[] = {
	{ mod_file_init, mod_file_shutdown }
};

#define NUM_DEFAULT_MODULES (sizeof(default_modules) / sizeof(module_spec))

static native_function_spec native_functions[] = {
	{ "Object.extend", (native_function) native_extend },
	{ "Object.clone", (native_function) native_clone },
	{ "Object.is_true", (native_function) native_is_true },
	{ "Object.to_string", (native_function) native_to_string },
	{ "String.to_string", (native_function) native_string_to_string },
	{ "String.concat", (native_function) native_string_concat },
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
	{ "xor", (native_function) native_xor }
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
	CURRENT_RUNTIME = r;
	r->mem = mem_create();
	r->loaded_modules = NULL;

	r->object_root = NULL;
	r->object_root = hval_hash_create(r);
	mem_add_gc_root(r->mem, r->object_root);

	r->top_level = hval_hash_create(r);
	hstr *str = hstr_create("Object");
	hval_hash_put(r->top_level, str, r->object_root, r->mem);
	hstr_release(str);
	mem_add_gc_root(r->mem, r->top_level);

	// init built-in types early
	init_module(r, mod_list_init);
	init_module(r, mod_object_init);

	// create a separate gc root for primitives creating while parsing
	// input. these primitives don't start with any other references,
	// so this is necessary to keep them from being collected.
	r->primitive_pool = (list_hval *) hval_list_create(r);
	mem_add_gc_root(r->mem, (hval *) r->primitive_pool);
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

	for (int i = 0; i < NUM_DEFAULT_MODULES; i++) {
		default_modules[i].shutdown(r);
	}

	hlog("releasing top_level: %p\n", r->top_level);
	mem_remove_gc_root(r->mem, r->top_level);
	r->top_level = NULL;

	mem_remove_gc_root(r->mem, r->object_root);
	r->object_root = NULL;

	mem_remove_gc_root(r->mem, (hval *) r->primitive_pool);
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

	register_native_functions(r, native_functions, sizeof(native_functions) / sizeof(native_function_spec));
	for (int i = 0; i < NUM_DEFAULT_MODULES; i++) {
		init_module(r, default_modules[i].init);
	}
	/*init_module(r, mod_file_init);*/
}

void init_module(runtime *rt, module_initializer init) {
	native_function_spec *functions = NULL;
	int count = 0;
	init(rt, &functions, &count);
	if (functions != NULL && count != 0) {
		register_native_functions(rt, functions, count);
	}
}

void register_native_functions(runtime *r, native_function_spec *spec, int count) {
	hval *func_val = NULL;
	for (native_function_spec *max = spec + count; spec < max; spec++) {
		func_val = hval_native_function_create(spec->function, r);
		register_builtin_r(r, r->top_level, spec->path, func_val);
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
	lexer *lexer = lexer_create(input);
	token *t = lexer_get_next_token(lexer);
	if (t == NULL) {
		*terminated = true;
		return NULL;
	}

	hval *result = NULL;
	expression *expr = read_complete_expression(lexer);
	if (expr != NULL) {
		result = runtime_evaluate_expression(runtime, expr, runtime->top_level);
		expr_destroy(expr, false, runtime->mem);
	} else {
		*terminated = true;
	}

	lexer_destroy(lexer, false);
	return result;
}

hval *runtime_exec(runtime *runtime, lexer_input *input)
{
	__current_runtime = runtime;
	hlog("analyzing...\n");
	lexer *lexer = lexer_create(input);
	/*runtime->input = input;*/
	expression *expr = runtime_analyze(runtime, lexer);
	lexer_destroy(lexer, false);

	hlog("creating main context\n");
	mem_add_gc_root(runtime->mem, runtime->top_level);

	hlog("beginning evaluation\n");
	hval *ret = runtime_evaluate_expression(runtime, expr, runtime->top_level);
	hlog("runtime_eval complete - got return value %p\n", ret);

	expr_destroy(expr, true, runtime->mem);
	expr = NULL;

	/*char *str = hval_to_string(ret);*/
	/*hlog("eval got result: %s", str);*/
	/*free(str);*/

	return runtime->top_level;
}

hval *runtime_load_module(runtime *runtime, lexer_input *input)
{
	lexer *lexer = lexer_create(input);
	expression *expr = runtime_analyze(runtime, lexer);
	lexer_destroy(lexer, false);
	if (runtime->loaded_modules == NULL) {
		runtime->loaded_modules = ll_create();
	}

	ll_insert_head(runtime->loaded_modules, expr);

	hval *ret = runtime_evaluate_expression(runtime, expr, runtime->top_level);
	return ret;
}

expression *runtime_analyze(runtime *rt, lexer *lexer)
{
	token *t = NULL;
	expression *expr_list = expr_create(expr_list_t);
	expr_list->operation.expr_list = ll_create();

	expression *expr = NULL;
	while ((t = lexer_get_next_token(lexer)) != NULL)
	{
		expr = read_complete_expression(lexer);
		if (expr == NULL)
		{
			runtime_error("read_complete_expression returned null\n");
			exit(1);
		}
		ll_insert_tail(expr_list->operation.expr_list, expr);
	}

	return expr_list;
}

expression *read_complete_expression(lexer *lexer)
{
	expression *expr = NULL;
	token_type tt = lexer->current->type;
	token *next;

	switch (tt)
	{
		case identifier:
			expr = read_identifier(lexer);
			break;
		case number:
			expr = read_number(lexer);
			break;
		case string:
			expr = read_string(lexer);
			break;
		case hash_start:
			expr = read_hash(lexer);
			break;
		case list_start:
			expr = read_list(lexer);
			next = lexer_peek_token(lexer);
			if (next && next->type == fn_declaration) {
				expr = read_function_declaration(lexer, expr);
			}
			break;
		case quote:
			expr = read_quoted(lexer);
			break;
		default:
			runtime_error("unhandled token type: %s\n", token_type_string(tt));
			break;
	}

	return expr;
}

static expression *read_function_declaration(lexer *lexer, expression *args) {
	/*token *pointy = lexer_get_next_token(rt);*/
	lexer_get_next_token(lexer); // consume the ->
	expect_token(lexer_current_token(lexer), fn_declaration);
	lexer_get_next_token(lexer);
	expect_token(lexer_current_token(lexer), list_start);
	expression *body = read_list(lexer);

	expression *fn = expr_create(expr_function_t);
	fn->operation.function_declaration = smalloc(sizeof(function_declaration));
	fn->operation.function_declaration->args = args;
	fn->operation.function_declaration->body = body;
	return fn;
}

expression *read_quoted(lexer *lexer)
{
	
	expression *expr = expr_create(expr_deferred_t);
	lexer_get_next_token(lexer);
	expression *deferred = read_complete_expression(lexer);
	expr->operation.deferred_expression = deferred;
	return expr;
}

expression *read_identifier(lexer *lexer)
{
	expression *expr = NULL;

	token *t = lexer->current;
	prop_ref *ref = malloc(sizeof(prop_ref));
	if (ref == NULL)
	{
		perror("read_identifier unable to allocate memory for prop_ref");
		exit(1);
	}

	ref->name = t->value.string;
	ref->site = NULL;
	hstr_retain(t->value.string);

	token *next = lexer_peek_token(lexer);
	if (next->type == assignment) {
		// consume the assignment and advance to the next
		lexer_get_next_token(lexer);
		lexer_get_next_token(lexer);
		expression *assgn = expr_create(expr_prop_set_t);
		assgn->operation.prop_set = malloc(sizeof(prop_set));
		assgn->operation.prop_set->ref = ref;
		assgn->operation.prop_set->value = read_complete_expression(lexer);
		expr = assgn;
	} else if (next->type == dereference) {
		hlog("reading dereference\n");
		// consume the assignment and advance to the next
		lexer_get_next_token(lexer);
		lexer_get_next_token(lexer);
		expr = read_complete_expression(lexer);
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
		lexer_get_next_token(lexer);
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
			inv->list_args = read_list(lexer);
			inv->hash_args = NULL;
		}
		else
		{
			inv->list_args = NULL;
			inv->hash_args = read_hash(lexer);
		}
		expr->operation.invocation = inv;
	} else {
		expr = expr_create(expr_prop_ref_t);
		expr->operation.prop_ref = ref;
	}

	return expr;
}

expression *read_string(lexer *lexer)
{
	token *t = lexer_current_token(lexer);
	expression *expr = expr_create(expr_primitive_t);
	expr->operation.primitive = hval_string_create(t->value.string, CURRENT_RUNTIME);
	hval_list_insert_head(CURRENT_RUNTIME->primitive_pool, expr->operation.primitive);
	return expr;
}

expression *read_number(lexer *lexer)
{
	token *t = lexer_current_token(lexer);
	expression *expr = expr_create(expr_primitive_t);
	expr->operation.primitive = hval_number_create(t->value.number, CURRENT_RUNTIME);
	hval_list_insert_head(CURRENT_RUNTIME->primitive_pool, expr->operation.primitive);
	return expr;
}

expression *read_list(lexer *lexer)
{
	expression *list = expr_create(expr_list_literal_t);
	list->operation.list_literal = ll_create();

	lexer_get_next_token(lexer);
	token *t = lexer_current_token(lexer);
	expression *expr = NULL;
	while (t != NULL && t->type != list_end)
	{
		/*fprintf(stderr, " read_list got token: %s\n", token_type_string(t->type));*/
		expr = read_complete_expression(lexer);
		ll_insert_tail(list->operation.list_literal, expr);
		t = lexer_get_next_token(lexer);
	}

	return list;
}

static expression *read_hash(lexer *lexer)
{
	expression *hash_lit = expr_create(expr_hash_literal_t);
	hash_lit->operation.hash_literal = hash_create((hash_function) hash_hstr, (key_comparator) hstr_comparator);
	// consume the hash_start
	lexer_get_next_token(lexer);
	token *t = lexer_current_token(lexer);
	while (t != NULL && t->type != hash_end)
	{
		expect_token(t, identifier);
		hstr *key = t->value.string;
		hstr_retain(key);
		t = lexer_get_next_token(lexer);
		expect_token(t, assignment);
		lexer_get_next_token(lexer);
		expression *value = read_complete_expression(lexer);
		hash_put(hash_lit->operation.hash_literal, key, value);
		t = lexer_get_next_token(lexer);
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
		case expr_function_t:
			return eval_expr_function_declaration(rt, expr->operation.function_declaration, context);
		default:
			hlog("Error: unknown expression type");
			exit(1);
	}
}

static hval *eval_expr_function_declaration(runtime *rt, function_declaration *decl, hval *context)
{
	hval *fn = hval_hash_create(rt);
	expression *expr = decl->body;
	hval *args = (hval *) eval_expr_function_args(rt, decl->args, false, context);
	hval_hash_put(fn, FN_ARGS, args, rt->mem);
	mem_remove_gc_root(rt->mem, args);
	hval *body = eval_expr_deferred(rt, expr, context);
	hval_hash_put(fn, FN_EXPR, body, rt->mem);

	return fn;
}

static list_hval *eval_expr_function_args(runtime *rt, expression *expr, bool for_invocation, hval *context) {
	list_hval *arglist = (list_hval *) hval_list_create(rt);
	mem_add_gc_root(rt->mem, (hval *) arglist);
	ll_node *arg_node = expr->operation.list_literal->head;
	expression *arg_expr = NULL;
	hval *arg = NULL;
	hval *value = NULL;
	prop_set *set = NULL;
	int i = 0;
	while (arg_node) {
		arg_expr = (expression *) arg_node->data;
		arg = hval_hash_create(rt);
		/*fprintf(stderr, "  created arg %d: %p\n", i, arg);*/
		hval_list_insert_tail(arglist, arg);
		switch (arg_expr->type) {
		case expr_prop_ref_t:
			if (for_invocation) {
				hval_hash_put(arg, VALUE, runtime_evaluate_expression(rt, arg_expr, context), rt->mem);
			} else {
				hval_hash_put(arg, NAME, hval_string_create(arg_expr->operation.prop_ref->name, rt), rt->mem);
			}
			break;
		case expr_prop_set_t:
			set = arg_expr->operation.prop_set;
			hval_hash_put(arg, NAME, hval_string_create(set->ref->name, rt), rt->mem);
			hval_hash_put(arg, VALUE, runtime_evaluate_expression(rt, set->value, context), rt->mem);
			break;
		case expr_primitive_t:
			hval_hash_put(arg, VALUE, arg_expr->operation.primitive, rt->mem);
			break;
		default:
			value = runtime_evaluate_expression(rt, arg_expr, context);
			hval_hash_put(arg, VALUE, value, rt->mem);
			value = NULL;
		}

		arg_node = arg_node->next;
		++i;
	}

	/*mem_add_gc_root(rt->mem, arglist);*/
	return arglist;
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
	list_hval *list = (list_hval *) hval_list_create(rt);
	mem_add_gc_root(rt->mem, (hval *) list);
	ll_node *current = expr_list->operation.list_literal->head;
	expression *expr = NULL;
	hval *result = NULL;
	while (current)
	{
		expr = (expression *) current->data;
		result = runtime_evaluate_expression(rt, expr, context);
		if (result) {
			hval_list_insert_tail(list, result);
			hval_release(result, rt->mem);
		}
		result = NULL;
		current = current->next;
	}

	mem_remove_gc_root(rt->mem, (hval *) list);

	return (hval *) list;
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
	hval *fn = runtime_evaluate_expression(rt, inv->function, context);
	/*fprintf(stderr, "================== eval_expr_invocation\n");*/
	mem_add_gc_root(rt->mem, fn);
	if (fn == NULL)
	{
		return NULL;
	}

	// coalesce the provided args and the defaults into a hash
	// for function invocation, order is irrelevent
	list_hval *in_args = eval_expr_function_args(rt, inv->list_args, true, context);
	/*mem_add_gc_root(rt->mem, in_args);*/
	hval *args = runtime_build_function_arguments(rt, fn, in_args);
	mem_add_gc_root(rt->mem, args);

	hval *result = runtime_call_function(rt, fn, args, context);
	mem_remove_gc_root(rt->mem, (hval *) in_args);
	mem_remove_gc_root(rt->mem, args);
	mem_remove_gc_root(rt->mem, fn);
	return result;
}

hval *runtime_build_function_arguments(runtime *rt, hval *fn, list_hval *in_args) {
	hval *args = NULL;
	/*mem_add_gc_root(rt->mem, fn);*/
	if (fn->type == native_function_t) {
		// Native functions can manually extract named functions,
		// but default values aren't supported yet.
		args = (hval *) in_args;
	} else {
		args = hval_hash_create(rt);
		list_hval *default_args = (list_hval *) hval_hash_get(fn, FN_ARGS, rt);
		hval *name = NULL;
		hval *value = NULL;
		ll_node *node;
		linked_list *unnamed = ll_create();

		if (in_args) {
			node = in_args->list->head;
			while (node) {
				value = runtime_get_arg_value(node);
				name = runtime_get_arg_name(node);
				if (name) {
					hval_hash_put(args, name->value.str, value, rt->mem);
				} else {
					ll_insert_tail(unnamed, value);
				}
				node = node->next;
			}
		}

		/*fprintf(stderr, "++++++++++++++++++ fn: %p\tdefault_args: %p\n", fn, default_args);*/
		node = default_args->list->head;
		ll_node *unnamed_node = unnamed->head;
		LL_FOREACH(default_args->list, defnode) {
			name = runtime_get_arg_name(defnode);
			value = hval_hash_get(args, name->value.str, rt);
			// if a named argument was already bound for this, skip it
			if (value) continue;

			// otherwise, if there are any remaining unbound args, consume one
			if (unnamed_node) {
				value = (hval *) unnamed_node->data;
				unnamed_node = unnamed_node->next;
				ll_remove_first(unnamed, value);
			} else {
				// or just use the default, which may be null
				value = runtime_get_arg_value(defnode);
			}

			if (value == NULL) {
				runtime_error("No value provided for parameter %s\n", name->value.str->str);
			}
			hval_hash_put(args, name->value.str, value, rt->mem);
		}

		ll_destroy(unnamed, NULL, NULL);
	}
	/*mem_remove_gc_root(rt->mem, fn);*/

	return args;
}

hval *runtime_call_function(runtime *rt, hval *fn, hval *args, hval *context)
{
	/*static int gc_count = 0;*/
	if (!args &&  fn->type == native_function_t) {
		args = hval_list_create(rt);
	}

	if (args) {
		mem_add_gc_root(rt->mem, args);
	}

	hval *result = NULL;
	if (fn->type == native_function_t) {
		hval *self = hval_get_self(fn);
		result = fn->value.native_fn(self, args);
	} else {
		result = eval_expr_folly_invocation(rt, fn, args, context);
	}

	if (args) {
		mem_remove_gc_root(rt->mem, args);
	}
	/*if (++gc_count == 1000) {*/
		/*gc_count = 0;*/
		/*gc_with_temp_root(rt->mem, result);*/
	/*}*/
	return result;
}

hval *runtime_call_hnamed_function(runtime *rt, hstr *name, hval *site, list_hval *args, hval *context) {
	hval *func = hval_hash_get(site, name, rt);
	hval *prepared_args = runtime_build_function_arguments(rt, func, args);
	return runtime_call_function(rt, func, prepared_args, context);
	//return runtime_call_function(rt, func, args, context);
}

static hval *eval_expr_folly_invocation(runtime *rt, hval *fn, hval *args, hval *context)
{
	hval *expr = hval_hash_get(fn, FN_EXPR, rt);
	hval *fn_context = hval_hash_create_child(expr->value.deferred_expression.ctx, rt);
	mem_add_gc_root(rt->mem, fn_context);
	if (args->type == hash_t) {
		hval_hash_put_all(fn_context, args, rt->mem);
		hval *self = hval_get_self(fn);
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

/*token *runtime_peek_token(runtime *runtime)*/
/*{*/
	/*if (!runtime->peek) {*/
		/*runtime->peek = get_next_token(runtime->input);*/
	/*}*/

	/*return runtime->peek;*/
/*}*/

/*token *lexer_get_next_token(runtime *runtime)*/
/*{*/
	/*if (runtime->current) {*/
		/*token_destroy(runtime->current, NULL);*/
	/*}*/

	/*token *t = NULL;*/
	/*if (runtime->peek) {*/
		/*t = runtime->peek;*/
		/*runtime->peek = NULL;*/
	/*} else {*/
		/*t = get_next_token(runtime->input);*/
	/*}*/

	/*runtime->current = t;*/
	/*return t;*/
/*}*/

static void expect_token(token *t, type token_type)
{
	if (!t || token_type != t->type)
	{
		hlog("Error: unexpected %s (expected %s)",
			token_type_string_token(t),
			token_type_string(token_type));
	}
}

NATIVE_FUNCTION(native_print)
{
	hstr *name = hstr_create("to_string");
	hval *str = NULL;
	if (args->type == hash_t) {
		printf("can't print a hash yet");
	} else {
		ll_node *node = ((list_hval *)args)->list->head;
		hval *arg = NULL;
		bool printed_any = false;
		while (node) {
			if (printed_any) {
				printf(" ");
			}
			printed_any = true;

			arg = runtime_get_arg_value(node);
			str = runtime_call_hnamed_function(CURRENT_RUNTIME, name, arg, NULL, CURRENT_RUNTIME->top_level);
			fputs(str->value.str->str, stdout);
			hval_release(str, CURRENT_RUNTIME->mem);
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

NATIVE_FUNCTION(native_add)
{
	int sum = 0;
	ll_node *current = ((list_hval *)args)->list->head;
	hval *item = NULL;
	while (current) {
		item = runtime_get_arg_value(current);
		sum += item->value.number;
		current = current->next;
	}

	return hval_number_create(sum, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(native_subtract)
{
	int val = 0;
	linked_list *arglist = hval_list_list(args);
	hval *hv = NULL;
	if (arglist->size == 0) {
		val = 0;
	} else if (arglist->size == 1) {
		hv = runtime_get_arg_value(arglist->head);
		val = -hval_number_value(hv);
	} else 	{
		hv = runtime_get_arg_value(arglist->head);
		val = hval_number_value(hv);
		ll_node *current = arglist->head->next;
		while (current) {
			hv = runtime_get_arg_value(current);
			val = val - hval_number_value(hv);
			current = current->next;
		}
	}

	return hval_number_create(val, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(native_equals) {
	if (args->type != list_t || hval_list_size(args) < 2) {
		runtime_error("native_equals: arguments must be a list with at least 2 elements\n");
	}

	// TODO Make this polymorphic, using an = method on objects
	hval *ref = runtime_get_arg_value(hval_list_head(args));
	hval *candidate = NULL;
	bool equals = true;
	ll_node *node = hval_list_head(args)->next;
	while (node && equals) {
		candidate = runtime_get_arg_value(node);
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

	return hval_number_create(equals ? 1 : 0, CURRENT_RUNTIME);
}

static NATIVE_FUNCTION(native_is_true)
{
	return hval_hash_get(CURRENT_RUNTIME->top_level, hval_is_true(this) ? TRUE : FALSE, CURRENT_RUNTIME);
}

static NATIVE_FUNCTION(native_lt)
{
	hval *arg1 = NULL;
	hval *arg2 = NULL;
	extract_arg_list(CURRENT_RUNTIME, args, &arg1, number_t, &arg2, number_t, NULL);
	
	bool lt = arg1->value.number < arg2->value.number;
	return hval_number_create(lt ? 1 : 0, CURRENT_RUNTIME);
}

static NATIVE_FUNCTION(native_gt)
{
	hval *arg1 = NULL;
	hval *arg2 = NULL;
	extract_arg_list(CURRENT_RUNTIME, args, &arg1, number_t, &arg2, number_t, NULL);
	bool gt = arg1->value.number > arg2->value.number;
	return hval_number_create(gt ? 1 : 0, CURRENT_RUNTIME);
}

static NATIVE_FUNCTION(native_fn)
{
	hval *fn = hval_hash_create(CURRENT_RUNTIME);

	hval_hash_put(fn, FN_ARGS, hval_list_head_hval(args), CURRENT_RUNTIME->mem);
	hval_hash_put(fn, FN_EXPR, hval_list_tail_hval(args), CURRENT_RUNTIME->mem);
	return fn;
}

static NATIVE_FUNCTION(native_clone)
{
	return hval_clone(this, CURRENT_RUNTIME);
}

static NATIVE_FUNCTION(native_extend)
{
	hlog("native_extend: %p\n", this);
	hval *sub = hval_hash_create_child(this, CURRENT_RUNTIME);
	hash_iterator *iter = hash_iterator_create(args->members);
	while (iter->current_key) {
		if (iter->current_key != PARENT) {
			hval_hash_put(sub, iter->current_key, iter->current_value, CURRENT_RUNTIME->mem);
		}
		hash_iterator_next(iter);
	}

	hash_iterator_destroy(iter);
	return sub;

}

static NATIVE_FUNCTION(native_to_string)
{
	char *str = hval_to_string(this);
	hstr *hs = hstr_create(str);
	free(str);
	str = NULL;

	hval *obj = hval_string_create(hs, CURRENT_RUNTIME);
	hstr_release(hs);
	return obj;
}

static NATIVE_FUNCTION(native_string_to_string) {
	return this;
}

static NATIVE_FUNCTION(native_number_to_string)
{
	char *str = fmt("%d", this->value.number);
	hstr *hs = hstr_create(str);
	free(str);
	str = NULL;
	hval *obj = hval_string_create(hs, CURRENT_RUNTIME);
	hstr_release(hs);
	return obj;
}

static NATIVE_FUNCTION(native_cond)
{
	if (args->type != list_t) {
		runtime_error("native_cond: argument mismatch: expected list");
	}

	ll_node *test_node = hval_list_head(args);
	while (test_node) {
		/*hval *cond_hval = (hval *) test_node->data;*/
		/*fprintf(stderr, " getting arg value from %p %p\n", test_node, test_node->data);*/
		list_hval *cond_hval = (list_hval *) runtime_get_arg_value(test_node);
		/*fprintf(stderr, " test_node: %p %p; cond_hval: %p\n", test_node, test_node->data, cond_hval);*/
		linked_list *cond_pair = cond_hval->list;

		hval *test = undefer(CURRENT_RUNTIME, hval_list_head_hval(cond_pair));

		if (hval_is_true(test)) {
			return cond_pair->head != cond_pair->tail ? undefer(CURRENT_RUNTIME, (hval *) cond_pair->tail->data) : test;
			break;
		}

		test_node = test_node->next;
	}
	fprintf(stderr, "-- native_cond %p done", args);
	
	return NULL;
}

NATIVE_FUNCTION(native_while)
{
	hval *test = NULL;
	hval *body = NULL;
	extract_arg_list(CURRENT_RUNTIME, args, &test, deferred_expression_t, &body, deferred_expression_t, NULL);

	hval *result = NULL;
	while (hval_is_true(undefer(CURRENT_RUNTIME, test))) {
		result = undefer(CURRENT_RUNTIME, body);
		/*hval *cond = undefer(test);*/
	}

	return result;
}

static hval *undefer(runtime *rt, hval *maybe_deferred) {
	mem_add_gc_root(CURRENT_RUNTIME->mem, maybe_deferred);
	if (maybe_deferred->type == deferred_expression_t) {
		deferred_expression *def = &(maybe_deferred->value.deferred_expression);
		hval *result = runtime_evaluate_expression(CURRENT_RUNTIME, def->expr, def->ctx);
		mem_remove_gc_root(CURRENT_RUNTIME->mem, maybe_deferred);
		return result;
	}

	mem_remove_gc_root(CURRENT_RUNTIME->mem, maybe_deferred);
	return maybe_deferred;
}

NATIVE_FUNCTION(native_and)
{
	if (args->type != list_t) {
		runtime_error("argument mismatch: expected list\n");
	}

	ll_node *node = hval_list_head(args);
	bool result = true;
	while (node && result) {
		hval *current = runtime_get_arg_value(node);
		current = undefer(CURRENT_RUNTIME, current);
		if (!hval_is_true(current)) {
			result = false;
		}
		node = node->next;
	}

	return hval_number_create(result ? 1 : 0, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(native_or)
{
	if (args->type != list_t) {
		runtime_error("argument mismatch: expected list\n");
	}

	ll_node *node = hval_list_head(args);
	bool result = false;
	while (node && !result) {
		hval *current = runtime_get_arg_value(node);
		current = undefer(CURRENT_RUNTIME, current);
		if (hval_is_true(current)) {
			result = true;
		}
		node = node->next;
	}

	return hval_number_create(result ? 1 : 0, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(native_not)
{
	if (args->type != list_t) {
		runtime_error("argument mismatch: expected list\n");
	}

	if (hval_list_size(args) != 1) {
		runtime_error("argument count mismatch: not() accepts exactly 1\n");
	}

	hval *value = runtime_get_arg_value(hval_list_head(args));
	return hval_number_create(hval_is_true(value) ? 0 : 1, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(native_xor)
{
	if (args->type != list_t) {
		runtime_error("argument mismatch: expected list\n");
	}
	ll_node *node = hval_list_head(args);
	int truths = 0;
	while (node) {
		hval *val = runtime_get_arg_value(node);
		if (hval_is_true(val)) {
			++truths;
			if (truths > 1) {
				break;
			}
		}

		node = node->next;
	}

	return hval_number_create(truths == 1 ? 1 : 0, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(native_load)
{
	hval *file = NULL;
	hval *context = CURRENT_RUNTIME->top_level;
	/*if (args->type == list_t) {*/
	extract_arg_list(CURRENT_RUNTIME, args, &file, string_t, NULL);
	/*} else {*/
		/*static hstr *key_filename, *key_into;*/
		/*if (key_filename == NULL) key_filename = hstr_create("file");*/
		/*if (key_into == NULL) key_into = hstr_create("into");*/

		/*file = hval_hash_get(args, key_filename, CURRENT_RUNTIME);*/
		/*context = hval_hash_get(args, key_into, CURRENT_RUNTIME);*/
	/*}*/

	char *filename = file->value.str->str;
	struct stat stat_buf;
	int err = stat(filename, &stat_buf);
	if (err == -1) {
		perror("Unable to open file");
		return hval_hash_get(CURRENT_RUNTIME->top_level, FALSE, CURRENT_RUNTIME);
	} else if (!S_ISREG(stat_buf.st_mode)) {
		return hval_hash_get(CURRENT_RUNTIME->top_level, FALSE, CURRENT_RUNTIME);
	}

	lexer_input *input = lexer_file_input_create(filename);
	runtime_load_module(CURRENT_RUNTIME, input);
	lexer_input_destroy(input);
	return hval_hash_get(CURRENT_RUNTIME->top_level, TRUE, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(native_show_heap)
{
	debug_heap_output(CURRENT_RUNTIME->mem);
	return NULL;
	/*printf("heap size: %s bytes in %d chunks\n", CURRENT_RUNTIME->mem->*/
}

static NATIVE_FUNCTION(native_string_concat)
{
	hstr *name = hstr_create("to_string");
	buffer *buf = buffer_create(128);
	hval *arg = NULL, *arg_str;
	/*char *arg_str = NULL;*/
	LL_FOREACH(hval_list_list(args), arg_node) {
		arg = runtime_get_arg_value(arg_node);
		arg_str = runtime_call_hnamed_function(CURRENT_RUNTIME, name, arg, NULL, CURRENT_RUNTIME->top_level);
		/*arg_str = hval_to_string(arg);*/
		buffer_append_string(buf, arg_str->value.str->str);
		hval_release(arg_str, CURRENT_RUNTIME->mem);
	}

	hstr *hs = hstr_create(buf->data);
	hval *str = hval_string_create(hs, CURRENT_RUNTIME);
	hstr_release(hs);
	buffer_destroy(buf);

	hstr_release(name);
	return str;
}
