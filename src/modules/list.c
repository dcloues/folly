#include "list.h"
#include "data.h"

native_function_spec list_module_functions[] = {
	{ "List.clone", mod_list_clone },
	{ "List.pop", mod_list_pop },
	{ "List.push", mod_list_push },
	{ "List.foreach", mod_list_foreach }
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

