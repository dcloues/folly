#include <stdbool.h>
#include "buffer.h"
#include "linked_list.h"

typedef enum { identifier, number, string, assignment, list_start, list_end, hash_start, hash_end, delim } token_type;

typedef union {
	char *string;
	int number;
	linked_list *list;
} value;

typedef struct {
	token_type type;
	value value;
} token;

typedef struct {
	bool (*test_input)(char, buffer *);
	token *(*read_token)(FILE *fh, buffer *);
} rule;

const char* token_type_string(token *token);
char* token_to_string(token *token);

token* get_next_token(FILE *fh);
token* get_token_number(FILE *fh);
token *token_create(token_type type);
