#include <stdlib.h>
#include <string.h>
#include "type.h"
#include "ht.h"
#include "ht_builtins.h"

hval *hval_string_create(char *str)
{
	hval *hval = malloc(sizeof(hval));
	hval->value.str.chars = str;
	hval->value.str.len = strlen(str);
	return hval;
}

hval *hval_number_create(int number)
{
	hval *hval = malloc(sizeof(hval));
	hval->value.number = number;
	return hval;
}

hval *hval_hash_create(void)
{
	hval *hval = malloc(sizeof(hval));
	hval->value.hash.members = hash_create(hash_string, hash_string_comparator);
	return hval;
}

char *hval_to_string(hval *hval)
{
	return "hval";
}

