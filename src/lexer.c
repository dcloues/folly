#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lexer.h"
#include "buffer.h"

size_t token_string_size(token *token);
void read_matching(FILE *fh, buffer *buf, bool (*matcher)(char, buffer*));

token *get_token_numeric(FILE *fh, buffer *buf);
token *get_token_string(FILE *fh, buffer *buf);
token *get_token_identifier(FILE *fh, buffer *buf);
token *get_token_assignment(FILE *fh, buffer *buf);

bool is_numeric(const char c, buffer *buffer);
bool is_string_delim(const char c, buffer *buffer);
bool is_identifier(const char c, buffer *buffer);
bool is_whitespace(const char c, buffer *buffer);
bool is_string_incomplete(const char c, buffer *buf);
bool is_assignment(const char c, buffer *buffer);

static rule rules[] = {
	{is_whitespace},
	{is_numeric, get_token_numeric},
	{is_string_delim, get_token_string},
	{is_identifier, get_token_identifier},
	{is_assignment, get_token_assignment}
};

token *token_create(token_type type)
{
	token *token = malloc(sizeof(token));
	if (!token) {
		perror("Unable to allocate memory for token");
		exit(1);
	}

	token->type = type;
	return token;
}

const char* token_type_string(token *token)
{
	switch (token->type) {
		case identifier:
			return "identifier";
		case string:
			return "string";
		case number:
			return "number";
		case assignment:
			return "assignment";
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
			break;
		default:
			sprintf(buf, "%s", type);
			break;
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
		int i = 0;
		rule *r = NULL;
		bool matched = false;
		token *token = NULL;
		for (; i < sizeof(rules); i++)
		{
			r = rules + i;
			if (r->test_input(c, NULL))
			{
				matched = true;
				break;
			}
		}

		if (!matched)
		{
			printf("Error: unexpected %c' (%d)", c, ch);
			exit(1);
		}
		else if (r->read_token)
		{
			// if no read op, the rule produces no output and should be skipped
			buffer *buf = buffer_create(512);
			buffer_append_char(buf, c);
			token = r->read_token(fh, buf);
			buffer_destroy(buf);

			return token;
		}
	}
}

token *get_token_numeric(FILE *fh, buffer *buf)
{
	read_matching(fh, buf, is_numeric);
	int value = atoi(buf->data);
	token *token = malloc(sizeof(token));
	token->type = number;
	token->value.number = value;
	return token;
}

token *get_token_string(FILE *fh, buffer *buf)
{
	read_matching(fh, buf, is_string_incomplete);
	char *str = buffer_substring(buf, 1, buf->len - 2);

	token *token = token_create(string);
	token->value.string = str;
	return token;
}

token *get_token_identifier(FILE *fh, buffer *buf)
{
	read_matching(fh, buf, is_identifier);
	token *token = token_create(identifier);
	token->value.string = buffer_to_string(buf);

	return token;
}

token *get_token_assignment(FILE *fh, buffer *buf)
{
	return token_create(assignment);
}

void read_matching(FILE *fh, buffer *buf, bool (*matcher)(char, buffer*))
{
	while (!feof(fh)) {
		int ch = fgetc(fh);
		char c = (char) ch;
		if (ch == -1) 
		{
			return;
		}
		if (matcher(ch, buf)) {
			buffer_append_char(buf, c);
		} else {
			ungetc(ch, fh);
			return;
		}
	}
}

bool is_numeric(const char c, buffer *buf)
{
	return c >= '0' && c <= '9';
}

bool is_string_delim(const char c, buffer *buf)
{
	return c == '\'' || c == '"';
}

bool is_string_incomplete(const char c, buffer *buf)
{
	if (buf->len < 2) {
		return true;
	}

	int len = buf->len;
	return !(buffer_peek(buf) == buf->data[0] && buf->data[len-2] != '\\');
}

bool is_whitespace(const char c, buffer *buf)
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

inline bool is_identifier(const char c, buffer *buf)
{
	return (isalnum(c) || ispunct(c)) && !is_assignment(c, buf);
}

inline bool is_assignment(const char c, buffer *buf)
{
	return c == ':';
}
