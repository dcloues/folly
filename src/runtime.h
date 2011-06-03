#include "type.h"
#include "lexer.h"
#include "string.h"

#ifndef RUNTIME_H
#define RUNTIME_H

typedef struct {
	linked_list *tokens;
	ll_node *current;
	hval *top_level;
	hval *last_result;
} runtime;

typedef enum { expr_prop_ref_t, expr_prop_set_t, expr_invocation_t, expr_list_literal_t, expr_hash_literal_t, expr_primitive_t, expr_list_t } expression_type;

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
	prop_ref *function;
	linked_list *list_args;
	hash *hash_args;
} invocation;

struct expression {
	expression_type type;
	union {
		prop_ref *prop_ref;
		prop_set *prop_set;
		linked_list *list_literal;
		hash *hash_literal;
		hval *primitive;
		invocation *invocation;
		linked_list *expr_list;
	} operation;
};

runtime *runtime_create();
void runtime_destroy();
hval *runtime_eval(runtime *runtime, char *file);
hval *runtime_eval_token(token *token, runtime *runtime, hval *context, hval *last_result);
hval *runtime_eval_identifier(token *token, runtime *runtime, hval *context);

#endif

