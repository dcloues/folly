#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include "data.h"
#include "type.h"
#include "runtime.h"

typedef struct _file_hval {
	hval base;
	FILE *fh;
} file_hval;

NATIVE_FUNCTION(mod_file_clone);
NATIVE_FUNCTION(mod_file_open);
NATIVE_FUNCTION(mod_file_close);
NATIVE_FUNCTION(mod_file_eof);
NATIVE_FUNCTION(mod_file_read_line);

void mod_file_init(runtime *, native_function_spec **functions, int *function_count);
void mod_file_shutdown(runtime *);

#endif
