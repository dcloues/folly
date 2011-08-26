#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <assert.h>
#include "buffer.h"
#include "fmt.h"
#include "ht.h"
#include "ht_builtins.h"
#include "linked_list.h"
#include "log.h"
#include "mm.h"
#include "type.h"

typedef struct {
	mem *mem;
	bool destroy_hvals;
} expr_destructor_context;

static char *hval_hash_to_string(hash *h);
static char *hval_list_to_string(linked_list *h);
void print_hash_member(hash *h, hstr *key, hval *value, buffer *b);
static void prop_ref_destroy(prop_ref *ref, bool destroy_hvals, mem *m);

#if HVAL_STATS
static int hval_create_count = 0;
static int hval_child_count = 0;
static int hval_clone_count = 0;
static int hval_reuse_count = 0;
#endif

void type_init_globals()
{
	FN_SELF = hstr_create("self");
	FN_ARGS = hstr_create("__args__");
	FN_EXPR = hstr_create("__expr__");
	PARENT = hstr_create("__parent__");
	STRING = hstr_create("String");
	NUMBER = hstr_create("Number");
	BOOLEAN = hstr_create("Boolean");
	TRUE = hstr_create("true");
	FALSE = hstr_create("false");
	NAME = hstr_create("name");
	VALUE = hstr_create("value");
}

void type_destroy_globals()
{
	hstr_release(FN_SELF);
	hstr_release(FN_ARGS);
	hstr_release(FN_EXPR);
	hstr_release(PARENT);
	hstr_release(STRING);
	hstr_release(NUMBER);
	hstr_release(BOOLEAN);
	hstr_release(TRUE);
	hstr_release(FALSE);
	hstr_release(NAME);
	hstr_release(VALUE);
}

const char *hval_type_string(type t)
{
	switch (t)
	{
		case string_t:			return "string";
		case number_t:			return "number";
		case list_t:			return "list";
		case hash_t:			return "hash";
		case native_function_t:		return "native function";
		case deferred_expression_t:	return "deferred expression";
		default:			return "unknown";
	}
}

hval *hval_clone(hval *val, runtime *rt) {
	hlog("hval_clone: %p\n", val);
#if HVAL_STATS
	hval_clone_count++;
#endif
	hval *clone = hval_create(val->type, rt);
	switch (val->type) {
	case hash_t:
		hval_clone_hash(val, clone, rt);
		break;
	case native_function_t:
		hval_clone_hash(val, clone, rt);
		clone->value.native_fn = val->value.native_fn;
		break;
	default:
		printf("hval_clone() does not support type %s", hval_type_string(val->type));
		exit(1);
		break;
	}

	return clone;
}

void hval_clone_hash(hval *src, hval *dest, runtime *rt) {
	hash_iterator *iter = hash_iterator_create(src->members);
	while (iter->current_key) {
		hval *value = iter->current_value;

		if (value && hval_is_callable(value) && (hval_get_self(value) == src || hval_get_self(value) == NULL)) {
			/*fprintf(stderr, "bind func %p; default args %p\n", value, hval_hash_get(value, FN_ARGS, rt));*/
			value = hval_clone(value, rt);
			hval_bind_function(value, dest, rt->mem);
		}
		hval_hash_put(dest, iter->current_key, value, rt->mem);

		hash_iterator_next(iter);
	}

	hash_iterator_destroy(iter);
}

hval *hval_string_create(hstr *str, runtime *rt)
{
	hstr_retain(str);
	hval *hv = hval_hash_create_child(hval_hash_get(rt->top_level, STRING, rt), rt);
	hv->type = string_t;
	hv->value.str = str;
	return hv;
}

hval *hval_number_create(int number, runtime *rt)
{
	hval *hv = hval_hash_create_child(hval_hash_get(rt->top_level, NUMBER, rt), rt);
	hv->type = number_t;
	/*hval *hv = hval_create(number_t, rt);*/
	hv->value.number = number;
	return hv;
}

hval *hval_boolean_create(bool value, runtime *rt)
{
	hval *hv = hval_hash_create_child(hval_hash_get(rt->top_level, BOOLEAN, rt), rt);
	hv->value.boolean = value;
	hv->type = boolean_t;
	return hv;
}

hval *hval_hash_create(runtime *rt)
{
	return hval_create(hash_t, rt);
}

hval *hval_hash_create_child(hval *parent, runtime *rt)
{
#if HVAL_STATS
	hval_child_count++;
#endif
	hval *hv = hval_hash_create(rt);
	hval_hash_put(hv, PARENT, parent, rt->mem);
	return hv;
}

hval *hval_hash_get(hval *hv, hstr *key, runtime *rt)
{
	/*hlog("hval_hash_get: %s (%p)\n", key->str, hv);*/
	if (hv == NULL)
	{
		hlog("NULL hash - returning\n");
		return NULL;
	}

	hash *h = hv->members;
	hval *val = hash_get(h, key);
	if (val == NULL)
	{
		hval *parent = hash_get(h, PARENT);
		val = hval_hash_get(parent, key, rt);

		hval *self = hval_get_self(val);
		if (rt && val && hval_is_callable(val) && (self == NULL || (self == parent && self != rt->top_level))) {
			val = hval_clone(val, rt);
			hval_bind_function(val, hv, rt->mem);
			hval_hash_put(hv, key, val, rt->mem);
		}

	}

	return val;
}

hval *hval_hash_put(hval *hv, hstr *key, hval *value, mem *m)
{
	hlog("hval_hash_put: %p %s -> %p\n", hv, key->str, value);
	hstr_retain(key);
	if (value != NULL)
	{
		hval_retain(value);
	}

	hval *previous = hash_get(hv->members, key);
	hash_put(hv->members, key, value);
	if (previous != NULL)
	{
		hval_release(previous, m);
		hstr_release(key);
	}

	return value;
}

hval *hval_list_create(runtime *rt)
{
	hval *hv = hval_create(list_t, rt);
	hv->value.list = ll_create();
	assert(hv->value.list != NULL);
	return hv;
}

void hval_list_insert_tail(hval *list, hval *val)
{
	if (val)
	{
		hval_retain(val);
	}

	ll_insert_tail(list->value.list, val);
}

void hval_list_insert_head(hval *list, hval *val)
{
	if (val)
	{
		hval_retain(val);
	}
	assert(list->value.list != NULL);
	ll_insert_head(list->value.list, val);
}

hval *hval_native_function_create(native_function fn, runtime *rt)
{
	hval *hv = hval_create(native_function_t, rt);
	hv->value.native_fn = fn;
	return hv;
}

hval *hval_bind_function(hval *function, hval *site, mem *m)
{
	hval_hash_put(function, FN_SELF, site, m);
	return function;
}

bool hval_is_callable(hval *test)
{
	return test != NULL &&
		(test->type == native_function_t
		|| (hval_hash_get(test, FN_EXPR, NULL) != NULL && hval_hash_get(test, FN_ARGS, NULL) != NULL));
}

hval *hval_get_self(hval *function)
{
	return hval_hash_get(function, FN_SELF, NULL);
}

char *hval_to_string(hval *hval)
{
	if (hval == NULL)
	{
		return fmt("(null hval)");
	}

	const type t = hval->type;
	const char *type_str = hval_type_string(t);
	char *contents = NULL;
	char *str = NULL;
	switch (t)
	{
		case string_t:
			return fmt("%s@%p: %s", type_str, hval, hval->value.str->str);
		case number_t:
			return fmt("%s@%p: %d", type_str, hval, hval->value.number);
		case hash_t:
			contents = hval_hash_to_string(hval->members);
			str = fmt("%s@%p: %s", type_str, hval, contents);
			free(contents);
			return str;
		case list_t:
			contents = hval_list_to_string(hval->value.list);
			str = fmt("%s@%p: %s", type_str, hval, contents);
			free(contents);
			return str;
		case native_function_t:
			return fmt("native function");
		case deferred_expression_t:
			return fmt("deferred expression");
		case boolean_t:
			return fmt("boolean (%s)", hval->value.boolean ? "true" : "false");
		default:
			return fmt("[?]");

	}

	return "hval_to_string_error";
}

char *hval_hash_to_string(hash *h)
{
	buffer *b = buffer_create(128);
	buffer_printf(b, "size: %d {", h->size);

	/*hash_iterate(h, (key_value_callback) print_hash_member, b);*/
	/*if (h->size > 0)*/
	/*{*/
		/*buffer_shrink(b, 1);*/
	/*}*/
	buffer_append_char(b, '}');

	char *str = buffer_to_string(b);
	buffer_destroy(b);
	b = NULL;
	return str;
}

static char *hval_list_to_string(linked_list *l)
{
	buffer *b = buffer_create(128);
	ll_node *node = l->head;
	buffer_append_char(b, '(');
	while (node != NULL)
	{
		char *member = hval_to_string(node->data);
		buffer_append_string(b, member);
		free(member);
		member = NULL;

		node = node->next;
		if (node)
		{
			buffer_append_char(b, ' ');
		}
	}

	buffer_append_char(b, ')');
	char *str = buffer_to_string(b);
	buffer_destroy(b);
	b = NULL;
	return str;
}

void print_hash_member(hash *h, hstr *key, hval *value, buffer *b)
{
	char *val = hval_to_string(value);
	if (val != NULL)
	{
		buffer_printf(b, "%s: %s ", key->str, val);
		free(val);
		val = NULL;
	}
}

hval *hval_create(type t, runtime *rt)
{
	return hval_create_custom(sizeof(hval), t, rt);
}

hval *hval_create_custom(size_t size, type hval_type, runtime *rt)
{
#if HVAL_STATS
	hval_create_count++;
#endif
	hval *hv = mem_alloc(sizeof(hval), rt->mem);
	if (hv->members == NULL) {
		hv->members = hash_create((hash_function) hash_hstr, (key_comparator) hstr_comparator);
#ifdef HVAL_STATS
	} else {
		hval_reuse_count++;
#endif
	}
	if (rt->object_root) {
		hval_hash_put(hv, PARENT, rt->object_root, rt->mem);
	} else {
		hlog("hval_create: ROOT OBJECT NULL");
	}
	hv->type = hval_type;
	hv->refs = 1;
	hv->reachable = false;
	hlog("hval_create: %p: %s\n", hv, hval_type_string(hval_type));
	return hv;
}

void hval_retain(hval *hv)
{
	hv->refs++;
}

void hval_release(hval *hv, mem *m)
{
	hv->refs--;
	hlog("hval_release: %p %d\n", hv, hv->refs);
	if (hv->refs == 0)
	{
		hval_destroy(hv, m, true);
	}
}

void hval_destroy(hval *hv, mem *m, bool recursive)
{
	hlog("hval_destroy: %p %s\n", hv, hval_type_string(hv->type));
	switch (hv->type)
	{
		case boolean_t:
		case native_function_t:
		case number_t:
			break;
		case string_t:
			hstr_release(hv->value.str);
			hv->value.str = NULL;
			break;
		case list_t:
			if (recursive) {
				ll_destroy(hv->value.list, (destructor) hval_release, NULL);
			} else {
				ll_destroy(hv->value.list, NULL, NULL);
			}
			break;
		case hash_t:
			break;
		case deferred_expression_t:
			if (recursive) {
				hval_release(hv->value.deferred_expression.ctx, m);
			}
			expr_destroy(hv->value.deferred_expression.expr, recursive, m);
			break;
		/*case function_t:*/
			/*free(hv->value.function_declaration);*/
			/*break;*/

	}

	if (recursive) {
		hash_destroy(hv->members, (destructor) hstr_release, NULL, (destructor) hval_release, m);
		hv->members = NULL;
	} else {
		/*hash_destroy(hv->members, (destructor) hstr_release, NULL, NULL, NULL);*/
		hash_empty(hv->members, (destructor) hstr_release, NULL, NULL, NULL);
	}

	mem_free(m, hv);
}



int hash_hstr(hstr *hs)
{
	if (!hs->hash_calculated) {
		hs->hash = hash_string(hs->str);
		hs->hash_calculated = true;
	}
	return hs->hash;
}

expression *expr_create(expression_type type)
{
	expression *expr = malloc(sizeof(expression));
	if (expr == NULL)
	{
		perror("Unable to allocate memory for expression");
		exit(1);
	}
	expr->refs = 1;
	expr->type = type;

	return expr;
}

void expr_retain(expression *expr)
{
	expr->refs++;
}

void expr_destructor(expression *expr, void *context) {
	expr_destructor_context *ctx = (expr_destructor_context *) context;

	expr_destroy(expr, ctx->destroy_hvals, ctx->mem);
}

void expr_destroy(expression *expr, bool destroy_hvals, mem *m)
{
	expr->refs--;
	if (expr->refs > 0)
	{
		return;
	}

	expr_destructor_context dest_context = { m, destroy_hvals };

	hlog("expr_destroy %p %d\n", expr, expr->type);
	switch (expr->type)
	{
		case expr_prop_ref_t:
			prop_ref_destroy(expr->operation.prop_ref, destroy_hvals, m);
			break;
		case expr_prop_set_t:
			prop_ref_destroy(expr->operation.prop_set->ref, destroy_hvals, m);
			expr_destroy(expr->operation.prop_set->value, destroy_hvals, m);
			free(expr->operation.prop_set);
			break;
		case expr_list_literal_t:
			ll_destroy(expr->operation.list_literal, (destructor) expr_destructor, &dest_context);
			break;
		case expr_list_t:
			ll_destroy(expr->operation.expr_list, (destructor) expr_destructor, &dest_context);
			break;
		case expr_primitive_t:
			if (destroy_hvals) {
				hval_release(expr->operation.primitive, m);
			}
			break;
		case expr_hash_literal_t:
			hash_destroy(expr->operation.hash_literal, (destructor) hstr_release, NULL, (destructor) expr_destructor, &dest_context);
			break;
		case expr_invocation_t:
			if (expr->operation.invocation->list_args != NULL) {
				expr_destroy(expr->operation.invocation->list_args, destroy_hvals, m);
			} else if (expr->operation.invocation->hash_args != NULL) {
				expr_destroy(expr->operation.invocation->hash_args, destroy_hvals, m);
			}
			expr_destroy(expr->operation.invocation->function, destroy_hvals, m);
			free(expr->operation.invocation);
			break;
		case expr_deferred_t:
			expr_destroy(expr->operation.deferred_expression, destroy_hvals, m);
			break;
		case expr_function_t:
			expr_destroy(expr->operation.function_declaration->args, destroy_hvals, m);
			expr_destroy(expr->operation.function_declaration->body, destroy_hvals, m);
			free(expr->operation.function_declaration);
			break;
		default:
			hlog("ERROR: unexpected type passed to expr_destroy\n");
			break;
	}

	free(expr);
}

static void prop_ref_destroy(prop_ref *ref, bool destroy_hvals, mem *m)
{
	if (ref->site)
	{
		expr_destroy(ref->site, destroy_hvals, m);
	}

	hstr_release(ref->name);
	free(ref);
}

hval *hval_hash_put_all(hval *dest, hval *src, mem *m)
{
	hash_iterator *iter = hash_iterator_create(src->members);
	while (iter->current_key != NULL) {
		if (iter->current_key != PARENT) {
			hval_hash_put(dest, iter->current_key, iter->current_value, m);
		}
		hash_iterator_next(iter);
	}

	hash_iterator_destroy(iter);
	return dest;
}

bool hval_is_true(hval *test) {
	if (test == NULL) {
		return false;
	}

	switch (test->type) {
	case number_t:
		return test->value.number != 0;
	case string_t:
		return strcasecmp("true", test->value.str->str) == 0;
	case boolean_t:
		return test->value.boolean;
	default:
		return true;
	}
}

#if HVAL_STATS
void print_hval_stats()
{
	const char *fmt = " %10d %s\n";
	fprintf(stderr, "hval statistics\n");
	fprintf(stderr, fmt, hval_create_count, "hvals created");
	fprintf(stderr, fmt, hval_reuse_count, "hvals reused");
	fprintf(stderr, fmt, hval_clone_count, "hvals cloned");
	fprintf(stderr, fmt, hval_child_count, "hval children created");
}
#endif
