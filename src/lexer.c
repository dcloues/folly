#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lexer.h"
#include "lexer_io.h"
#include "log.h"
#include "buffer.h"
#include "smalloc.h"

size_t token_string_size(token *token);
void read_matching(lexer_input *li, buffer *buf, bool (*matcher)(char, buffer*));

#define lexer_error(...) fputs("lexer error: ", stderr);\
fprintf(stderr, __VA_ARGS__);\
exit(1);

token *get_token_numeric(lexer_input *li, buffer *buf);
token *get_token_string(lexer_input *li, buffer *buf);
token *get_token_identifier(lexer_input *li, buffer *buf);
token *get_token_assignment(lexer_input *li, buffer *buf);
token *get_token_list_start(lexer_input *li, buffer *buf);
token *get_token_list_end(lexer_input *li, buffer *buf);
token *get_token_hash_start(lexer_input *li, buffer *buf);
token *get_token_hash_end(lexer_input *li, buffer *buf);
token *get_token_delim(lexer_input *li, buffer *buf);
token *get_token_quote(lexer_input *li, buffer *buf);
token *get_token_dereference(lexer_input *li, buffer *buf);

bool is_numeric(const char c, buffer *buffer);
bool is_string_delim(const char c, buffer *buffer);
bool is_identifier(const char c, buffer *buffer);
bool is_whitespace(const char c, buffer *buffer);
bool is_string_incomplete(const char c, buffer *buf);
bool is_assignment(const char c, buffer *buffer);
bool is_list_start(const char c, buffer *buffer);
bool is_list_end(const char c, buffer *buffer);
bool is_hash_start(const char c, buffer *buffer);
bool is_hash_end(const char c, buffer *buffer);
bool is_delim(const char c, buffer *buffer);
bool is_quote(const char c, buffer *buffer);
bool is_dereference(const char c, buffer *buffer);

static rule rules[] = {
	{is_whitespace},
	{is_numeric, get_token_numeric},
	{is_string_delim, get_token_string},
	{is_identifier, get_token_identifier},
	{is_assignment, get_token_assignment},
	{is_list_start, get_token_list_start},
	{is_list_end, get_token_list_end},
	{is_hash_start, get_token_hash_start},
	{is_hash_end, get_token_hash_end},
	{is_delim, get_token_delim},
	{is_quote, get_token_quote},
	{is_dereference, get_token_dereference}
};

token *token_create(token_type type)
{
	token *t = malloc(sizeof(token));
	if (!t) {
		hlog("Unable to allocate memory for token");
		exit(1);
	}

	t->type = type;
	return t;
}

void token_destroy(token *t, void *context)
{
	if (t)
	{
		if (t->type == identifier || t->type == string)
		{
			hstr_release(t->value.string);
		}
		free(t);
	}
}

const char *token_type_string_token(token *token)
{
	if (token != NULL)
	{
		return token_type_string(token->type);
	}

	return "(null)";
}

const char* token_type_string(token_type type)
{
	switch (type) {
		case identifier:
			return "identifier";
		case string:
			return "string";
		case number:
			return "number";
		case assignment:
			return "assignment";
		case list_start:
			return "list_start";
		case list_end:
			return "list_end";
		case hash_start:
			return "hash_start";
		case hash_end:
			return "hash_end";
		case delim:
			return "delim";
		case fn_declaration:
			return "->";
		default:
			return "[unknown]";
	}
}

char *token_to_string(token *token)
{
	const char *type = token_type_string_token(token);
	size_t data_size = token_string_size(token);
	data_size += strlen(type);
	data_size += 3; // 2 for ': ', 1 for \0
	char *buf = malloc(data_size);
	switch (token->type) {
		case identifier:
		case string:
			sprintf(buf, "%s: %s", type, token->value.string->str);
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
			return strlen(token->value.string->str);
		case number:
			return 11;
		default:
			lexer_error("unknown token type for token_string_type: %s", token_type_string(token->type));
	}
}

token* get_next_token(lexer_input *li)
{
	while (true)
	{
		int ch = lexer_getc(li);
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
			hlog("Error: unexpected %c' (%d)", c, ch);
			exit(1);
		}
		else if (r->read_token)
		{
			// if no read op, the rule produces no output and should be skipped
			buffer *buf = buffer_create(512);
			buffer_append_char(buf, c);
			token = r->read_token(li, buf);
			buffer_destroy(buf);

			return token;
		}
	}

	return NULL;
}

token *get_token_numeric(lexer_input *li, buffer *buf)
{
	read_matching(li, buf, is_numeric);
	int value = atoi(buf->data);
	token *t = malloc(sizeof(token));
	t->type = number;
	t->value.number = value;
	return t;
}

token *get_token_string(lexer_input *li, buffer *buf)
{
	read_matching(li, buf, is_string_incomplete);
	char *str = buffer_substring(buf, 1, buf->len - 2);
	/*printf("get_token_string read %d chars: '%s'\n", buf->len-2, str);*/

	token *token = token_create(string);
	token->value.string = hstr_create(str);
	free(str);
	return token;
}

token *get_token_identifier(lexer_input *li, buffer *buf)
{
	read_matching(li, buf, is_identifier);
	char *str = buffer_to_string(buf);
	/*fprintf(stderr, "get_token_identifier: %s\n", str);*/
	if (strcmp("->", str) == 0) {
		free(str);
		return token_create(fn_declaration);
	}

	token *token = token_create(identifier);
	token->value.string = hstr_create(str);
	free(str);

	return token;
}

token *get_token_assignment(lexer_input *li, buffer *buf)
{
	return token_create(assignment);
}

token *get_token_list_start(lexer_input *li, buffer *buf)
{
	return token_create(list_start);
}

token *get_token_list_end(lexer_input *li, buffer *buf)
{
	return token_create(list_end);
}

token *get_token_hash_start(lexer_input *li, buffer *buf)
{
	return token_create(hash_start);
}

token *get_token_hash_end(lexer_input *li, buffer *buf)
{
	return token_create(hash_end);
}

token *get_token_delim(lexer_input *li, buffer *buf)
{
	return token_create(delim);
}

token *get_token_quote(lexer_input *li, buffer *buf)
{
	return token_create(quote);
}

token *get_token_dereference(lexer_input *li, buffer *buf)
{
	return token_create(dereference);
}

void read_matching(lexer_input *li, buffer *buf, bool (*matcher)(char, buffer*))
{
	while (true) {
		int ch = lexer_getc(li);
		char c = (char) ch;
		if (ch == -1) 
		{
			return;
		}
		if (matcher(ch, buf)) {
			buffer_append_char(buf, c);
		} else {
			lexer_ungetc(ch, li);
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
	return (isalnum(c) || ispunct(c))
		&& !is_assignment(c, buf)
		&& !is_list_start(c, buf)
		&& !is_list_end(c, buf)
		&& !is_hash_start(c, buf)
		&& !is_hash_end(c, buf)
		&& !is_delim(c, buf)
		&& !is_quote(c, buf)
		&& !is_dereference(c, buf);
}

inline bool is_assignment(const char c, buffer *buf)
{
	return c == ':';
}

inline bool is_list_start(const char c, buffer *buf)
{
	return c == '(';
}

inline bool is_list_end(const char c, buffer *buf)
{
	return c == ')';
}


inline bool is_hash_start(const char c, buffer *buffer)
{
	return c == '{';
}

inline bool is_hash_end(const char c, buffer *buffer)
{
	return c == '}';
}

inline bool is_delim(const char c, buffer *buffer)
{
	return c == ',';
}

inline bool is_quote(const char c, buffer *buffer)
{
	return c == '`';
}

inline bool is_dereference(const char c, buffer *buffer)
{
	return c == '.';
}


lexer *lexer_create(lexer_input *input)
{
	lexer *l = smalloc(sizeof(lexer));

	l->input = input;
	l->current = l->peek = NULL;

	return l;
}
void lexer_destroy(lexer *l, bool destroy_input)
{
	if (destroy_input) {
		lexer_input_destroy(l->input);
		l->input = NULL;
	}

	l->current = l->peek = NULL;
	free(l);
}

token *lexer_get_next_token(lexer *l)
{
	if (l->current) {
		token_destroy(l->current, NULL);
	}

	token *t = NULL;
	if (l->peek) {
		t = l->peek;
		l->peek = NULL;
	} else {
		t = get_next_token(l->input);
	}

	l->current = t;
	/*fprintf(stderr, "token: %s\n", t ? token_type_string(t->type) : "NULL");*/
	return t;
}

token *lexer_current_token(lexer *l)
{
	return l->current;
}

token *lexer_peek_token(lexer *l)
{
	if (!l->peek) {
		l->peek = get_next_token(l->input);
	}

	/*fprintf(stderr, "lexer_peek_token: %s\n", l->peek ? token_type_string(l->peek->type) : "NULL");*/
	return l->peek;
}
