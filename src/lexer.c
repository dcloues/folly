#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lexer.h"

size_t token_string_size(token *token);

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
