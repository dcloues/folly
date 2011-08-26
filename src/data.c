#include <stdio.h>
#include <stdarg.h>
#include "data.h"
#include "ht.h"
#include "type.h"

void extract_arg_list(runtime *rt, hval *arglist, ...)
{
	if (arglist->type != list_t) {
		runtime_error("argument error: expected list\n");
	}

	va_list vargs;
	va_start(vargs, arglist);
	hval **dest = NULL;
	hval *value = NULL;
	type expected_type;
	ll_node *arglist_node = arglist->value.list->head;
	while ((dest = va_arg(vargs, hval**)) != NULL) {
		expected_type = va_arg(vargs, type);
		/*value = (hval *) arglist_node->data;*/
		value = runtime_get_arg_value(arglist_node);
		if (value->type == expected_type) {
			*dest = value;
		} else {
			runtime_error("argument error: got %s, expected %s\n", hval_type_string(expected_type), hval_type_string(value->type));
		}
		arglist_node = arglist_node->next;
	}

	va_end(vargs);
}
