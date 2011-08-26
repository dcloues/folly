#include <stdio.h>
#include "file.h"
#include "str.h"

static hstr *PATH;

#define file_is_open(hvf) (((file_hval *) hvf)->fh != NULL)
#define THIS_FILE ((file_hval *) this)
#define THIS_FILE_HANDLE (((file_hval *) this)->fh)

native_function_spec file_module_functions[] = {
	{ "File.open", mod_file_open },
	{ "File.clone", mod_file_clone },
	{ "File.close", mod_file_close },
	{ "File.eof", mod_file_eof },
	{ "File.read_line", mod_file_read_line }
};

void mod_file_init(runtime *rt, native_function_spec **functions, int *function_count)
{
	PATH = hstr_create("path");
	*functions = file_module_functions;
	*function_count = sizeof(file_module_functions) / sizeof(native_function_spec);
}

void mod_file_shutdown(runtime *rt)
{
	hstr_release(PATH);
}

void mod_file_destroy(runtime *rt)
{
	hstr_release(PATH);
}

NATIVE_FUNCTION(mod_file_clone)
{
	hval *file = hval_create_custom(sizeof(file_hval), hash_t, CURRENT_RUNTIME);
	hval_clone_hash(this, file, CURRENT_RUNTIME);
	// TODO Handle cloning an open file

	return file;
}

NATIVE_FUNCTION(mod_file_open)
{
	hval *mode = NULL;
	extract_arg_list(CURRENT_RUNTIME, args, &mode, string_t, NULL);
	hval *path = hval_hash_get(this, PATH, NULL);

	if (path == NULL) {
		return hval_hash_get(CURRENT_RUNTIME->top_level, FALSE, NULL);
	}

	file_hval *f = (file_hval *) this;
	f->fh = fopen(path->value.str->str, mode->value.str->str);
	// TODO error checking

	/*fprintf(stderr, "opening file (%s): %s\n", mode->value.str->str, path->value.str->str);*/
	return hval_boolean_create(true, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(mod_file_close)
{
	if (!file_is_open(this)) {
		return hval_boolean_create(false, CURRENT_RUNTIME);
	}

	int result = fclose(THIS_FILE_HANDLE);
	THIS_FILE_HANDLE = NULL;
	if (result == 0) {
		return hval_boolean_create(true, CURRENT_RUNTIME);
	}

	return hval_boolean_create(false, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(mod_file_eof)
{
	file_hval *f = (file_hval *) this;
	if (f->fh) {
		int eof = feof(f->fh);
		return hval_boolean_create(eof != 0, CURRENT_RUNTIME);
	}

	return hval_boolean_create(true, CURRENT_RUNTIME);
}

NATIVE_FUNCTION(mod_file_read_line)
{
	file_hval *f = (file_hval *) this;
	if (!f->fh) {
		runtime_error("file not open; cannot read");
	}

	char buf[1024];
	char *ret = fgets(buf, sizeof(buf), f->fh);
	if (ret == NULL) {
		buf[0] = '\0';
	} else {
		int last_index = strlen(buf)-1;
		if (last_index > -1 && buf[last_index] == '\n') {
			buf[last_index] = '\0';
		}
		/*buf[strlen(buf)-1] = '\0';*/
	}

	hstr *str = hstr_create(buf);
	hval *strval = hval_string_create(str, CURRENT_RUNTIME);
	hstr_release(str);
	return strval;
}
