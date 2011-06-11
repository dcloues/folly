#ifndef TYPE_H
#define TYPE_H

#include "linked_list.h"
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
	hash *hash_args;
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

typedef hval *(*native_function)(hval *this, hval *args);

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
		union hash {
			hash *members;
			hash *parent;
		} hash;
		native_function native_fn;
	} value;
};

hval *hval_create(type);
void hval_retain(hval *hv);
void hval_release(hval *hv);
hval *hval_string_create(hstr *str);
hval *hval_number_create(int num);
hval *hval_list_create();
hval *hval_hash_create(void);
hval *hval_hash_create_child(hval *parent);
hval *hval_hash_get(hval *hv, hstr *str);
hval *hval_hash_put(hval *hv, hstr *str, hval *value);
hval *hval_native_function_create(native_function fn);
void hval_list_insert_head(hval *list, hval *val);
void hval_list_insert_tail(hval *list, hval *val);
char *hval_to_string(hval *);
const char *hval_type_string(type t);
int hash_hstr(hstr *);
expression *expr_create(expression_type);
void expr_retain(expression *);
void expr_destroy(expression *);

#endif

