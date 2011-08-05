#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "buffer.h"
#include "fmt.h"
#include "ht.h"
#include "ht_builtins.h"
#include "linked_list.h"
#include "log.h"
#include "mm.h"
#include "type.h"

static char *hval_hash_to_string(hash *h);
static char *hval_list_to_string(linked_list *h);
void print_hash_member(hash *h, hstr *key, hval *value, buffer *b);
static void prop_ref_destroy(prop_ref *ref, mem *m);
static void hval_clone_hash(hval *src, hval *dest, mem *mem);

void type_init_globals()
{
	FN_SELF = hstr_create("self");
	FN_ARGS = hstr_create("__args__");
	FN_EXPR = hstr_create("__expr__");
	PARENT = hstr_create("__parent__");
}

void type_destroy_globals()
{
	hstr_release(FN_SELF);
	hstr_release(FN_ARGS);
	hstr_release(FN_EXPR);
	hstr_release(PARENT);
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
	hval *clone = hval_create(val->type, rt);
	switch (val->type) {
	case hash_t:
		hval_clone_hash(val, clone, rt->mem);
		break;
	default:
		hlog("hval_clone() does not support type %s", hval_type_string(val->type));
		exit(1);
		break;
	}

	return clone;
}

void hval_clone_hash(hval *src, hval *dest, mem *mem) {
	hash_iterator *iter = hash_iterator_create(src->members);
	while (iter->current_key) {
		char *str = hstr_to_str(iter->current_key);
		hlog("clone key: %s\n", str);
		free(str);

		hval *value = iter->current_value;
		if (value && hval_is_callable(value)) {
			hval_bind_function(value, dest, mem);
		}
		hval_hash_put(dest, iter->current_key, value, mem);

		hash_iterator_next(iter);
	}
}

hval *hval_string_create(hstr *str, runtime *rt)
{
	hstr_retain(str);
	hval *hv = hval_create(string_t, rt);
	hv->value.str = str;
	return hv;
}

hval *hval_number_create(int number, runtime *rt)
{
	hval *hv = hval_create(number_t, rt);
	hv->value.number = number;
	return hv;
}

hval *hval_hash_create(runtime *rt)
{
	return hval_create(hash_t, rt);
}

hval *hval_hash_create_child(hval *parent, runtime *rt)
{
	hlog("hval_hash_create_child\n");
	hval *hv = hval_hash_create(rt);
	hstr *key = hstr_create("__parent__");
	hval_hash_put(hv, key, parent, rt->mem);
	hstr_release(key);
	return hv;
}

hval *hval_hash_get(hval *hv, hstr *key, mem *mem)
{
	hlog("hval_hash_get: %s (%p)\n", key->str, hv);
	if (hv == NULL)
	{
		hlog("NULL hash - returning\n");
		return NULL;
	}

	hash *h = hv->members;
	hval *val = hash_get(h, key);
	if (val == NULL)
	{
		char *dump_str = hval_to_string(hv);
		hlog("%s not found in %s\n", key->str, dump_str);
		free(dump_str);
		hval *parent = hash_get(h, PARENT);
		val = hval_hash_get(hash_get(h, PARENT), key, mem);
		if (mem && val && hval_is_callable(val) && hval_get_self(val) == parent) {
			hval_bind_function(val, hv, mem);
		}

	}

	return val;
}

hval *hval_hash_put(hval *hv, hstr *key, hval *value, mem *m)
{
	// TODO Handle overwrite/cleanup
	char *str = hval_to_string(value);
	hlog("hval_hash_put: %p %s %p -> %p %s\n", hv, key->str, key, value, str);
	free(str);
	hstr_retain(key);
	if (value != NULL)
	{
		hval_retain(value);
	}

	hval *previous = hash_put(hv->members, key, value);
	if (previous != NULL)
	{
		str = hval_to_string(previous);
		hlog("previous value: %p %s\n", previous, str);
		free(str);
		hval_release(previous, m);
		hstr_release(key);
	}

	return value;
}

hval *hval_list_create(runtime *rt)
{
	hval *hv = hval_create(list_t, rt);
	hv->value.list = ll_create();
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

	}

	return "hval_to_string_error";
}

char *hval_hash_to_string(hash *h)
{
	buffer *b = buffer_create(128);
	buffer_printf(b, "size: %d {", h->size);

	hash_iterate(h, (key_value_callback) print_hash_member, b);
	if (h->size > 0)
	{
		buffer_shrink(b, 1);
	}
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

hval *hval_create(type hval_type, runtime *rt)
{
	hval *hv = mem_alloc(rt->mem);
	hv->members = hash_create((hash_function) hash_hstr, (key_comparator) hstr_comparator);
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
	/*hlog("hval_retain: %p: %d\n", hv, hv->refs);*/
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
		case string_t:
			hstr_release(hv->value.str);
			hv->value.str = NULL;
			break;
		case list_t:
			if (recursive) {
				ll_destroy(hv->value.list, (destructor) hval_release);
			} else {
				ll_destroy(hv->value.list, NULL);
			}
			break;
		case hash_t:
			break;
		case deferred_expression_t:
			if (recursive) {
				hval_release(hv->value.deferred_expression.ctx, m);
			}
			expr_destroy(hv->value.deferred_expression.expr, m);
			break;
	}

	void hval_release_helper(hval *val) {
		hval_release(val, m);
	}

	if (recursive) {
		hash_destroy(hv->members, (destructor) hstr_release, (destructor) hval_release_helper);
	} else {
		hash_destroy(hv->members, (destructor) hstr_release, NULL);
	}
	hv->members = NULL;

	mem_free(m, hv);
}

int hash_hstr(hstr *hs)
{
	return hash_string(hs->str);
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

void expr_destroy(expression *expr, mem *m)
{
	expr->refs--;
	if (expr->refs > 0)
	{
		return;
	}

	void expr_destructor(expression *to_destroy) {
		expr_destroy(to_destroy, m);
	}

	hlog("expr_destroy %p %d\n", expr, expr->type);
	switch (expr->type)
	{
		case expr_prop_ref_t:
			prop_ref_destroy(expr->operation.prop_ref, m);
			break;
		case expr_prop_set_t:
			prop_ref_destroy(expr->operation.prop_set->ref, m);
			expr_destroy(expr->operation.prop_set->value, m);
			free(expr->operation.prop_set);
			break;
		case expr_list_literal_t:
			ll_destroy(expr->operation.list_literal, (destructor) expr_destructor);
			break;
		case expr_list_t:
			ll_destroy(expr->operation.expr_list, (destructor) expr_destructor);
			break;
		case expr_primitive_t:
			hval_release(expr->operation.primitive, m);
			break;
		case expr_hash_literal_t:
			hash_destroy(expr->operation.hash_literal, (destructor) hstr_release, (destructor) expr_destructor);
			break;
		case expr_invocation_t:
			if (expr->operation.invocation->list_args != NULL) {
				expr_destroy(expr->operation.invocation->list_args, m);
			} else if (expr->operation.invocation->hash_args != NULL) {
				expr_destroy(expr->operation.invocation->hash_args, m);
			}
			expr_destroy(expr->operation.invocation->function, m);
			free(expr->operation.invocation);
			break;
		case expr_deferred_t:
			expr_destroy(expr->operation.deferred_expression, m);
			break;
		default:
			hlog("ERROR: unexpected type passed to expr_destroy\n");
			break;
	}

	free(expr);
}

static void prop_ref_destroy(prop_ref *ref, mem *m)
{
	if (ref->site)
	{
		expr_destroy(ref->site, m);
	}

	hstr_release(ref->name);
	free(ref);
}

hval *hval_hash_put_all(hval *dest, hval *src, mem *m)
{
	hash_iterator *iter = hash_iterator_create(src->members);
	while (iter->current_key != NULL) {
		hval_hash_put(dest, iter->current_key, iter->current_value, m);
		hash_iterator_next(iter);
	}

	hash_iterator_destroy(iter);
}

