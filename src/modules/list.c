#include "list.h"
#include "data.h"

native_function_spec list_module_functions[] = {
	{ "List.clone", mod_list_clone },
	{ "List.pop", mod_list_pop },
	{ "List.push", mod_list_push }
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
