#include <stdlib.h>
#include <stdio.h>
#include "smalloc.h"

void *
smalloc(size_t size)
{
	void *m = malloc(size);
	if (m == NULL) {
		perror("Unable to allocate memory");
		exit(1);
	}

	return m;
}
