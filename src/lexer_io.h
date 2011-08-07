#ifndef LEXER_IO_H
#define LEXER_IO_H
#include <stdio.h>

struct _lexer_input;
typedef struct _lexer_input lexer_input;

struct _lexer_input {
	int (*li_getc)(lexer_input *);
	int (*li_ungetc)(int c, lexer_input *);
	void (*li_destroy)(lexer_input *);
};

typedef struct {
	lexer_input base;
	FILE *fh;
} lexer_file_input;

#define lexer_getc(li) (li->li_getc(li))
#define lexer_ungetc(c, li) (li->li_ungetc(c, li))
#define lexer_input_destroy(li) (li->li_destroy(li))

lexer_input *
lexer_file_input_create(char *file);

#endif
