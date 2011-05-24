#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define HASH_STRING_LIMIT 32
int hash_string(void *vstr)
{
	const char *str = (char *) vstr;
	int hash = 0;

	int i = 0;
	unsigned char c;
	while (i < HASH_STRING_LIMIT && str[i])
	{
		c = (unsigned char) str[i];
		hash += c;
		i++;
	}

	return hash;
}

bool hash_string_comparator(void *str1, void *str2)
{
	return strcmp(str1, str2) == 0;
}
