#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lexer.h"

size_t token_string_size(token *token);
void read_matching(FILE *fh, char *buf, int *count, const int max, bool (*matcher)(char));
token *get_token_numeric(FILE *fh);

const char* token_type_string(token *token)
{
	switch (token->type) {
		case identifier:
			return "identifier";
		case string:
			return "string";
		case number:
			return "number";
		default:
			return "[unknown]";
	}
}

char *token_to_string(token *token)
{
	const char *type = token_type_string(token);	
	size_t data_size = token_string_size(token);
	data_size += strlen(type);
	data_size += 3; // 2 for ': ', 1 for \0
	char *buf = malloc(data_size);
	switch (token->type) {
		case identifier:
		case string:
			sprintf(buf, "%s: %s", type, token->value.string);
			break;
		case number:
			sprintf(buf, "%s: %d", type, token->value.number);
	}
	
	return buf;
}

size_t token_string_size(token *token)
{
	switch (token->type)
	{
		case identifier:
		case string:
			return strlen(token->value.string);
		case number:
			return 11;
	}
}

token* get_next_token(FILE *fh)
{
	while (!feof(fh))
	{
		int ch = fgetc(fh);	
		if (ch == -1) {
			return NULL;
		}

		char c = (char) ch;
		if (is_whitespace(c))
		{
			continue;
		}

		ungetc(c, fh);
		if (is_numeric(c))
		{
			return get_token_numeric(fh);
		}
		else
		{
			printf("don't know how to handle: '%c' (%d)", c, ch);
		}
	}
}

token *get_token_numeric(FILE *fh)
{
	char buf[8192];
	memset(buf,0, sizeof(buf));
	int count = 0;
	read_matching(fh, buf, &count, sizeof(buf), is_numeric);
	int value = atoi(buf);
	token *token = malloc(sizeof(token));
	token->type = number;
	token->value.number = value;
	return token;
}

void read_matching(FILE *fh, char *buf, int *count, int max, bool (*matcher)(char))
{
	while (!feof(fh) && *count < max) {
		int ch = fgetc(fh);
		char c = (char) ch;
		if (ch == -1) 
		{
			return;
		}
		if (matcher(ch)) {
			buf[*count] = ch;
			(*count)++;
		} else {
			ungetc(ch, fh);
			buf[*count] = '\0';
			(*count)++;
			return;
		}
	}
}

bool is_numeric(const char c)
{
	return c >= '0' && c <= '9';
}

bool is_whitespace(const char c)
{
	static char ws[] = {' ', '\t', '\n', '\r'};
	int i = sizeof(ws);
	while (i--) {
		if (c == ws[i]) {
			return true;
		}
	}
	return false;
}
