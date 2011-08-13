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
	bool trace = false;
	if (argc == 2) {
		input = lexer_file_input_create(argv[1]);
	} else {
		input = lexer_readline_input_create();
		trace = true;
	}

	if (trace) {
		hval *result = NULL;
		bool terminated = false;
		char *str = NULL;
		while (!terminated) {
			result = runtime_exec_one(r, input, &terminated);
			if (result) {
				str = hval_to_string(result);
				puts(str);
				putchar('\n');
				free(str);
			}
		}
	} else {
		runtime_exec(r, input);
	}

	lexer_input_destroy(input);
	input = NULL;

	runtime_destroy(r);
	r = NULL;

	runtime_destroy_globals();
	hlog_shutdown();

#if HVAL_STATS
	print_hval_stats();
#endif

	return 0;
}

