#include <stdbool.h>
#include "buffer.h"

typedef enum { identifier, number, string, assignment } token_type;

typedef union {
	char *string;
	int number;
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
