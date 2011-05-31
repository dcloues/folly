#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include "fmt.h"

char *fmt(char *format, ...)
{
	int size = 80;
	char *str = NULL, *new_str = NULL;
	int n = 0;
	va_list args;

	if ((str = malloc(size)) == NULL)
	{
		hlog("malloc failed in fmt");
		return NULL;
	}

	while (true)
	{
		va_start(args, format);
		n = vsnprintf(str, size, format, args);
		va_end(args);
		
		if (n > -1 && n < size)
		{
			return str;
		}
		else if (n > -1)
		{
			size = n + 1;
		}

		if ((new_str = realloc(str, size)) == NULL)
		{
			free(str);
			return NULL;
		}
		else
		{
			str = new_str;
		}
	}
}

