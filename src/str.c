#include "str.h"
#include <stdlib.h>
#include <string.h>
#include "smalloc.h"

hstr *hstr_create(char *chars)
{
	return hstr_create_len(chars, strlen(chars));
}

hstr *hstr_create_len(char *src, size_t size)
{
	hstr *hs = smalloc(sizeof(hstr) + size + 1);
	hstr_init(hs, src, size);
	return hs;
}

void hstr_init(hstr *hs, char *chars, size_t len)
{
	hs->refs = 1;
	hs->str[len] = '\0';
	hs->hash_calculated = false;
	hs->hash = 0;
	strncpy(hs->str, chars, len);
}

void hstr_retain(hstr *hs)
{
	hs->refs++;
}

void hstr_release(hstr *hs)
{
	hs->refs--;
	if (hs->refs == 0)
	{
		free(hs);
	}
}

char *hstr_to_str(hstr *hs)
{
	char *str = malloc(strlen(hs->str) + 1);
	strcpy(str, hs->str);
	return str;
}

bool hstr_comparator(hstr *h1, hstr *h2)
{
	return strcmp(h1->str, h2->str) == 0;
}
