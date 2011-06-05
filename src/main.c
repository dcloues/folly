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

	runtime *r = runtime_create();
	hval *ctx = runtime_eval(r, argv[1]);
	hlog("calling runtime_destroy\n");
	runtime_destroy(r);
	r = NULL;

	hlog("releasing context\n");
	hval_release(ctx);

	hlog_shutdown();

	return 0;
}
