#include <stdlib.h>
#include "type.h"

void *value_create(type t, size_t size)
{
	value *val = malloc(size);
	val->type = t;
	return val;
}

number *number_create(void)
{
	number *num = value_create(number_t, sizeof(number));
	return num;
}

string *string_create(void)
{
	string *str = value_create(string_t, sizeof(string));
	return str;
}

list *list_create(void)
{
	// TODO Error checking
	list *list = value_create(list_t, sizeof(list));
	list->data = malloc(sizeof(linked_list));
	return list;
}
