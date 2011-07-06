#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lexer.h"
#include "log.h"
#include "runtime.h"
#include "linked_list.h"

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("usage: parsify input\n");
		exit(1);
	}

	hlog_init("parsify.log");

	runtime_init_globals();
	runtime *r = runtime_create();
	hval *ctx = runtime_eval(r, argv[1]);
	hlog("calling runtime_destroy\n");

	hlog("releasing context\n");
	hval_release(ctx, r->mem);
	runtime_destroy(r);
	r = NULL;

	runtime_destroy_globals();

	hlog_shutdown();

	return 0;
}
