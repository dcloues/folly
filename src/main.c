#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lexer.h"
#include "lexer_io.h"
#include "log.h"
#include "runtime.h"
#include "linked_list.h"

int main(int argc, char **argv)
{
	hlog_init("parsify.log");

	runtime_init_globals();
	runtime *r = runtime_create();
	
	lexer_input *input = NULL;
	if (argc == 2) {
		input = lexer_file_input_create(argv[1]);
	} else {
		input = lexer_readline_input_create();
	}
	hval *ctx = runtime_exec(r, input);
	lexer_input_destroy(input);
	input = NULL;

	hlog("releasing context\n");
	hval_release(ctx, r->mem);
	runtime_destroy(r);
	r = NULL;

	runtime_destroy_globals();

	hlog_shutdown();

	return 0;
}

