#ifndef DATA_H
#define DATA_H

#include "ht.h"
#include "str.h"

typedef enum { string_t, number_t, hash_t, list_t, deferred_expression_t, native_function_t } type;
typedef enum { expr_prop_ref_t, expr_prop_set_t, expr_invocation_t, expr_list_literal_t, expr_hash_literal_t, expr_primitive_t, expr_list_t, expr_deferred_t } expression_type;

typedef struct hval hval;
typedef struct expression expression;

typedef struct prop_ref {
	expression *site;
	hstr *name;
} prop_ref;

typedef struct prop_set {
	prop_ref *ref;
	expression *value;
} prop_set;

typedef struct invocation {
	expression *function;
	expression *list_args;
	expression *hash_args;
} invocation;

struct expression {
	expression_type type;
	int refs;
	union {
		prop_ref *prop_ref;
		prop_set *prop_set;
		linked_list *list_literal;
		hash *hash_literal;
		hval *primitive;
		invocation *invocation;
		linked_list *expr_list;
		expression *deferred_expression;
	} operation;
};

typedef hval *(*native_function)(void *rt, hval *this, hval *args);

struct hval {
	type type;
	int refs;
	union {
		int number;
		hstr *str;
		linked_list *list;
		struct {
			hval *ctx;
			expression *expr;
		} deferred_expression;
		native_function native_fn;
	} value;
	hash *members;
	bool reachable;
};

#endif
