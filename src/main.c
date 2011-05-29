#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lexer.h"
#include "runtime.h"
#include "linked_list.h"

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("usage: parsify input\n");
		exit(1);
	}

	runtime *r = runtime_create();
	hval *ctx = runtime_eval(r, argv[1]);
	runtime_destroy(r);
	r = NULL;

	hval_release(ctx);

	return 0;
}
