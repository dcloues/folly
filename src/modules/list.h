#ifndef LIST_H
#define LIST_H

#include "data.h"
#include "type.h"
#include "runtime.h"

NATIVE_FUNCTION(mod_list_for_each);

struct list_hval {
	hval base;
	linked_list *list;
};

#define hval_list_size(hv) (((list_hval *)hv)->list->size)
#define hval_list_list(hv) (((list_hval *)hv)->list)
#define hval_list_head(hv) (((list_hval *)hv)->list->head)
#define hval_list_head_hval(hv) ((hval *) (((list_hval *)hv)->list->head->data))
#define hval_list_tail_hval(hv) ((hval *) (((list_hval *)hv)->list->tail->data))

void mod_list_init(runtime *, native_function_spec **functions, int *function_count);

NATIVE_FUNCTION(mod_list_clone);
NATIVE_FUNCTION(mod_list_push);
NATIVE_FUNCTION(mod_list_pop);
NATIVE_FUNCTION(mod_list_foreach);

#endif
