#include "lexer_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>

#include "smalloc.h"

static int lexer_file_input_getc(lexer_input *);
static int lexer_file_input_ungetc(int c, lexer_input *input);
static void lexer_file_input_destroy(lexer_input *input);

static int lexer_readline_input_getc(lexer_input *);
static int lexer_readline_input_ungetc(int c, lexer_input *input);
static void lexer_readline_input_destroy(lexer_input *input);

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

lexer_input *
lexer_readline_input_create()
{
	lexer_readline_input *input = smalloc(sizeof(lexer_readline_input));
	input->base.li_getc = lexer_readline_input_getc;
	input->base.li_ungetc = lexer_readline_input_ungetc;
	input->base.li_destroy = lexer_readline_input_destroy;

	input->buf = NULL;
	input->buf_size = 0;
	input->index = 0;

	return (lexer_input *) input;
}

static int
lexer_readline_input_getc(lexer_input *input)
{
	lexer_readline_input *lri = (lexer_readline_input *) input;

	if (lri->buf == NULL || lri->index >= lri->buf_size) {
		free(lri->buf);
		lri->buf = NULL;
		lri->buf_size = 0;
		lri->index = 0;
		char *line = NULL;
		do {
			if (line) {
				free(line);
				line = NULL;
			}

			line = readline(">");
			if (line && *line) {
				lri->buf = line;
				lri->buf_size = strlen(line);
			}
		} while (line && lri->buf == NULL);
	}

	int ch = EOF;
	if (lri->buf) {
		ch = lri->buf[lri->index];
		lri->index++;
	}

	return ch;
}

static int
lexer_readline_input_ungetc(int c, lexer_input *input)
{
	lexer_readline_input *lri = (lexer_readline_input *) input;
	if (lri->buf && lri->index > 0) {
		lri->index--;
	} else {
		fputs("Attempted to ungetc at index 0\n", stderr);
		exit(1);
	}

	return c;
}

static void
lexer_readline_input_destroy(lexer_input *input)
{
	lexer_readline_input *lri = (lexer_readline_input *) input;
	if (lri->buf) {
		free(lri->buf);
	}

	free(input);
}

