#include "object.h"
#include "runtime.h"
#include "type.h"

static native_function_spec object_functions[] = {
	{ "Object.eachpair", mod_object_eachpair }
};

void mod_object_init(runtime *runtime, native_function_spec **functions, int *function_count) {
	*functions = object_functions;
	*function_count = sizeof(object_functions) / sizeof(native_function_spec);
}

NATIVE_FUNCTION(mod_object_eachpair)
{
	if (hval_list_size(args) != 1) {
		return hval_boolean_create(false, CURRENT_RUNTIME);
	}

	hval *func = runtime_get_arg_value(hval_list_head(args));
	list_hval *arglist = (list_hval *) hval_list_create(CURRENT_RUNTIME);
	mem_add_gc_root(CURRENT_RUNTIME->mem, (hval *)arglist);
	hval *keywrap = hval_hash_create(CURRENT_RUNTIME);
	hval_list_insert_head(arglist, keywrap);
	hval *valwrap = hval_hash_create(CURRENT_RUNTIME);
	hval_list_insert_tail(arglist, valwrap);

	hval *reached_sentinel = hval_boolean_create(true, CURRENT_RUNTIME);
	hval *reached = hval_hash_create(CURRENT_RUNTIME);

	linked_list *ancestors = ll_create();
	ll_insert_head(ancestors, this->members);
	hash *current = NULL;
	while (ancestors->size) {
		current = ancestors->head->data;
		ll_remove_first(ancestors, current);
		hash_iterator *iter = hash_iterator_create(current);
		while (iter->current_key) {
			hstr *key = iter->current_key;
			if (strcmp(key->str, PARENT->str) == 0) {
				ll_insert_head(ancestors, ((hval *)iter->current_value)->members);
			} else if (hval_hash_get(reached, key, NULL) != reached_sentinel) {
				hval_hash_put(reached, key, reached_sentinel, NULL);
				hval *str = hval_string_create(iter->current_key, CURRENT_RUNTIME);
				hval_hash_put(keywrap, VALUE, str, NULL);
				hval_hash_put(valwrap, VALUE, iter->current_value, NULL);

				hval *prepared_args = runtime_build_function_arguments(CURRENT_RUNTIME, func, arglist);
				runtime_call_function(CURRENT_RUNTIME, func, prepared_args, CURRENT_RUNTIME->top_level);
				hval_release(prepared_args, CURRENT_RUNTIME->mem);
				hval_release(str, CURRENT_RUNTIME->mem);
			}

			hash_iterator_next(iter);
		}

		hash_iterator_destroy(iter);
		iter = NULL;
	}
	mem_remove_gc_root(CURRENT_RUNTIME->mem, (hval *)arglist);
	hval_release((hval *) arglist, CURRENT_RUNTIME->mem);

	return hval_boolean_create(true, CURRENT_RUNTIME);
}
