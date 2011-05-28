#include "str.h"
#include <stdlib.h>
#include <string.h>

hstr *hstr_create(char *chars)
{
	hstr *hs = malloc(sizeof(hstr));
	if (hs == NULL)
	{
		return NULL;
	}

	hstr_init(hs, chars);
	return hs;
}

void hstr_init(hstr *hs, char *chars)
{
	hs->refs = 1;
	hs->str = malloc(strlen(chars) + 1);
	if (hs->str != NULL)
	{
		strcpy(hs->str, chars);
	}
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

