#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "buffer.h"
#include "fmt.h"
#include "ht.h"
#include "ht_builtins.h"
#include "linked_list.h"
#include "type.h"

static char *hval_hash_to_string(hash *h);
void print_hash_member(hash *h, char *key, hval *value, buffer *b);
hval *hval_create(type);

const char *hval_type_string(type t)
{
	switch (t)
	{
		case string_t:	       	return "string";
		case number_t:	       	return "number";
		case list_t:	       	return "list";
		case hash_t:	       	return "hash";
		case quoted_list_t:	return "quoted list";
		default:		return "unknown";
	}
}

hval *hval_string_create(const char *str, const int len)
{
	hval *hv = hval_create(string_t);
	hv->value.str.chars = malloc(len + 1);
	if (!hv->value.str.chars)
	{
		perror("Unable to allocate memory for hval_string");
		exit(1);
	}

	strncpy(hv->value.str.chars, str, len + 1);
	hv->value.str.len = len;
	return hv;
}

hval *hval_number_create(int number)
{
	hval *hv = hval_create(number_t);
	hv->value.number = number;
	return hv;
}

hval *hval_hash_create(void)
{
	hval *hv = hval_create(hash_t);
	hv->value.hash.members = hash_create(hash_string, hash_string_comparator);
	return hv;
}

hval *hval_list_create(void)
{
	hval *hv = hval_create(list_t);
	hv->value.list = ll_create();
	return hv;
}

hval *hval_native_function_create(native_function fn)
{
	hval *hv = hval_hash_create();
	hash_put(hv->value.hash.members, "__nativefn__", fn);

	return hv;
}

char *hval_to_string(hval *hval)
{
	if (hval == NULL)
	{
		return "(NULL)";
	}

	const type t = hval->type;
	const char *type_str = hval_type_string(t);
	char *contents = NULL;
	char *str = NULL;
	switch (t)
	{
		case string_t:
			return fmt("%s@%p: %s", type_str, hval, hval->value.str.chars);
		case number_t:
			return fmt("%s@%p: %d", type_str, hval, hval->value.number);
		case hash_t:
			contents = hval_hash_to_string(hval->value.hash.members);
			str = fmt("%s@%p: %s", type_str, hval, contents);
			free(contents);
			return str;
	}

	return "hval_to_string_error";
}

char *hval_hash_to_string(hash *h)
{
	buffer *b = buffer_create(128);
	buffer_printf(b, "size: %d {", h->size);

	hash_iterate(h, (key_value_callback) print_hash_member, b);
	buffer_shrink(b, 1);
	buffer_append_char(b, '}');

	char *str = buffer_to_string(b);
	buffer_destroy(b);
	b = NULL;
	return str;
}

void print_hash_member(hash *h, char *key, hval *value, buffer *b)
{
	char *val = hval_to_string(value);
	if (val != NULL)
	{
		buffer_printf(b, "%s: %s ", key, val);
		free(val);
		val = NULL;
	}
}

hval *hval_create(type hval_type)
{
	hval *hv = malloc(sizeof(hval));
	hv->type = hval_type;
	return hv;
}

void hval_destroy(hval *hv)
{
	switch (hv->type)
	{
		case string_t:
			free(hv->value.str.chars);
			hv->value.str.chars = NULL;
			break;
		case hash_t:
			hash_destroy(hv->value.hash.members, free, (destructor)hval_destroy);
			hv->value.hash.members = NULL;
			break;
	}

	free(hv);
}
