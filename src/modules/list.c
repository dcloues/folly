#include "list.h"
#include "data.h"

native_function_spec list_module_functions[] = {
	{ "List.clone", mod_list_clone },
	{ "List.first", mod_list_first },
	{ "List.last", mod_list_last },
	{ "List.pop", mod_list_pop },
	{ "List.push", mod_list_push },
	{ "List.foreach", mod_list_foreach },
	{ "List.filter", mod_list_filter }
};

void mod_list_init(runtime *rt, native_function_spec **functions, int *function_count)
{
	*functions = list_module_functions;
	*function_count = sizeof(list_module_functions) / sizeof(native_function_spec);
}

NATIVE_FUNCTION(mod_list_clone)
{
	return hval_list_create(CURRENT_RUNTIME);
}

NATIVE_FUNCTION(mod_list_push)
{
	ll_node *argnode = hval_list_head(args);
	while (argnode) {
		hval_list_insert_head((list_hval *) this, runtime_get_arg_value(argnode));
		argnode = argnode->next;
	}

	return this;
}

NATIVE_FUNCTION(mod_list_pop)
{
	if (hval_list_size(this) == 0) {
		return hval_boolean_create(false, CURRENT_RUNTIME);
	}

	list_hval *this_list = (list_hval *) this;
	hval *value = hval_list_head(this_list)->data;
	ll_remove_first(this_list->list, value);
	return value;
}

NATIVE_FUNCTION(mod_list_foreach)
{
	if (hval_list_size(args) != 1) {
		return hval_boolean_create(false, CURRENT_RUNTIME);
	}

	hval *func = runtime_get_arg_value(hval_list_head(args));
	list_hval *arglist = (list_hval *) hval_list_create(CURRENT_RUNTIME);
	hval *argwrap = hval_hash_create(CURRENT_RUNTIME);
	hval_list_insert_head(arglist, argwrap);
	ll_node *node = ((list_hval *)this)->list->head;
	hval *prepared_args = NULL;
	while (node) {
		hval_hash_put(argwrap, VALUE, node->data, NULL);
		prepared_args = runtime_build_function_arguments(CURRENT_RUNTIME, func, arglist);
		runtime_call_function(CURRENT_RUNTIME, func, prepared_args, CURRENT_RUNTIME->top_level);

		node = node->next;
	}

	return hval_boolean_create(true, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(mod_list_first)
{
	if (hval_list_size(this) == 0) {
		return NULL;
	}
	
	return hval_list_head_hval(this);
}

NATIVE_FUNCTION(mod_list_last)
{
	if (hval_list_size(this) == 0) {
		return NULL;
	}

	return hval_list_tail_hval(this);
}

NATIVE_FUNCTION(mod_list_filter)
{
	if (hval_list_size(args) == 0 || hval_list_size(this) == 0) {
		return hval_list_create(CURRENT_RUNTIME);
	}

	hval *func = runtime_get_arg_value(hval_list_head(args));
	list_hval *filter_args = (list_hval *) hval_list_create(CURRENT_RUNTIME);
	hval *argwrap = hval_hash_create(CURRENT_RUNTIME);
	hval_list_insert_head(filter_args, argwrap);
	hval *prepared_args = NULL;
	list_hval *filtered = (list_hval *) hval_list_create(CURRENT_RUNTIME);
	LL_FOREACH(((list_hval *)this)->list, node) {
		hval *this_item = (hval *) node->data;
		hval_hash_put(argwrap, VALUE, this_item, NULL);
		prepared_args = runtime_build_function_arguments(CURRENT_RUNTIME, func, filter_args);
		hval *value = runtime_call_function(CURRENT_RUNTIME, func, prepared_args, CURRENT_RUNTIME->top_level);
		if (value != NULL && hval_is_true(value)) {
			hval_list_insert_tail(filtered, this_item);
		}
		hval_release(prepared_args, CURRENT_RUNTIME->mem);
		prepared_args = NULL;
	}

	return (hval *) filtered;
}
