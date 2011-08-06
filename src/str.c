#include "str.h"
#include <stdlib.h>
#include <string.h>

hstr *hstr_create(char *chars)
{
	return hstr_create_len(chars, strlen(chars));
}

hstr *hstr_create_len(char *src, size_t size)
{
	hstr *hs = malloc(sizeof(hstr));
	if (hs == NULL)
	{
		return NULL;
	}

	hstr_init(hs, src, size);
	return hs;
}

void hstr_init(hstr *hs, char *chars, size_t len)
{
	hs->refs = 1;
	hs->str = malloc(len + 1);
	hs->str[len] = '\0';
	if (hs->str != NULL)
	{
		strncpy(hs->str, chars, len);
	}
}

void hstr_retain(hstr *hs)
{
	printf("hstr_retain: %p (%d -> %d) '%s'\n", hs, hs->refs, hs->refs + 1, hs->str);
	hs->refs++;
}

void hstr_release(hstr *hs)
{
	printf("hstr_release: %p (%d -> %d) '%s'\n", hs, hs->refs, hs->refs - 1, hs->str);
	hs->refs--;
	if (hs->refs == 0)
	{
		free(hs->str);
		hs->str = NULL;
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
