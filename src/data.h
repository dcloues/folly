#ifndef DATA_H
#define DATA_H

#include "ht.h"
#include "str.h"

extern hstr *VALUE;
extern hstr *NAME;

typedef enum { free_t, string_t, number_t, hash_t, list_t, deferred_expression_t, native_function_t, boolean_t, function_t } type;
typedef enum { expr_prop_ref_t, expr_prop_set_t, expr_invocation_t, expr_list_literal_t, expr_hash_literal_t, expr_primitive_t, expr_list_t, expr_deferred_t, expr_function_t } expression_type;

typedef struct hval hval;
typedef struct list_hval list_hval;
typedef struct mem mem;
typedef struct expression expression;
//typedef struct _list_hval list_hval;

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

typedef struct _function_declaration {
	expression *args;
	expression *body;
} function_declaration;

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
		function_declaration *function_declaration;
	} operation;
};

typedef hval *(*native_function)(hval *this, hval *args);

typedef struct deferred_expression {
	hval *ctx;
	expression *expr;
} deferred_expression;

typedef struct function_decl {
	//hval *ctx;
	//hval *args;
	expression *body;
} function_decl;

struct hval {
	type type;
	int refs;
	union {
		int number;
		bool boolean;
		hstr *str;
		linked_list *list;
		deferred_expression deferred_expression;
		native_function native_fn;
		function_decl fn;
	} value;
	hash *members;
	bool reachable;
};

//struct list_hval {
	//hval base;
	//linked_list *list;
//};

typedef struct {
	hval *top_level;
	hval *last_result;
	mem *mem;
	list_hval *primitive_pool;
	hval *object_root;

	linked_list *loaded_modules;
} runtime;

typedef struct _native_function_spec {
	char *path;
	native_function function;
} native_function_spec;

typedef struct _chunk {
	int count;
	size_t element_size;
	size_t raw_size;
	char *free_hint;
	int allocated;
	linked_list *free_list;
	char base[];
} chunk;

typedef struct _chunk_list {
	chunk **chunks;
	int num_chunks;
} chunk_list;

struct mem {
	linked_list *gc_roots;
	chunk_list chunks[8];
	bool gc;
};

#define NATIVE_FUNCTION(name) hval *name(hval *this, hval *args)
#define runtime_error(...) fprintf(stderr, __VA_ARGS__); exit(1);
#define runtime_get_arg_value(lln) (hval_hash_get(((hval *)lln->data), VALUE, NULL))
#define runtime_get_arg_name(lln) (hval_hash_get(((hval *)lln->data), NAME, NULL))
void extract_arg_list(runtime *rt, hval *args, ...);
void register_native_functions(runtime *r, native_function_spec *spec, int count);

#endif
