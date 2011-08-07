#include "lexer_io.h"
#include <stdio.h>
#include <stdlib.h>

#include "smalloc.h"

static int lexer_file_input_getc(lexer_input *);
static int lexer_file_input_ungetc(int c, lexer_input *input);
static void lexer_file_input_destroy(lexer_input *input);

lexer_input *
lexer_file_input_create(char *file)
{
	FILE *fh = fopen(file, "r");
	if (fh == NULL) {
		perror("Unable to open file");
		exit(1);
	}

	lexer_file_input *input = smalloc(sizeof(lexer_file_input));
	input->fh = fh;
	input->base.li_getc = lexer_file_input_getc;
	input->base.li_ungetc = lexer_file_input_ungetc;
	input->base.li_destroy = lexer_file_input_destroy;

	return (lexer_input *) input;
}

static void lexer_file_input_destroy(lexer_input *input)
{
	lexer_file_input *lfi = (lexer_file_input *) input;
	fclose(lfi->fh);
	lfi->fh = NULL;
	free(lfi);
}

static int lexer_file_input_getc(lexer_input *input)
{
	lexer_file_input *lhf = (lexer_file_input *) input;
	return fgetc(lhf->fh);
}

static int lexer_file_input_ungetc(int c, lexer_input *input)
{
	lexer_file_input *lfi = (lexer_file_input *) input;
	return ungetc(c, lfi->fh);
}

